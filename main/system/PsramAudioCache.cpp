#include "system/PsramAudioCache.hpp"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <algorithm>
#include <cstdio>
#include <memory>
#include <utility>

static constexpr const char* TAG = "PsramAudioCache";

namespace InertialSaber::System {

namespace {

using Espressif::Wrappers::MemoryFile;

struct FileCloser {
    void operator()(FILE* file) const { fclose(file); }
};

constexpr std::string_view kHumName = "hum.wav";

std::string swingLowName(uint8_t pairIndex) {
    return "swingl" + std::to_string(pairIndex) + ".wav";
}

std::string swingHighName(uint8_t pairIndex) {
    return "swingh" + std::to_string(pairIndex) + ".wav";
}

std::string mountedPath(std::string_view name) {
    return std::string(PsramAudioCache::kMountPoint).append("/").append(name);
}

} // namespace

PsramAudioCache::PsramAudioCache(uint8_t maxFiles, uint8_t maxFds)
    : m_vfs(kMountPoint, maxFiles, maxFds) {
    m_registeredNames.reserve(maxFiles);
}

PsramAudioCache::~PsramAudioCache() {
    if (m_loaderTask) {
        vTaskDelete(m_loaderTask);
        m_loaderTask = nullptr;
    }
}

esp_err_t PsramAudioCache::init() {
    esp_err_t err = m_vfs.init();
    if (err != ESP_OK) return err;

    BaseType_t ret = xTaskCreatePinnedToCore(
        &PsramAudioCache::loaderTaskFn,
        "psram_loader",
        4096,
        this,
        2,
        &m_loaderTask,
        1
    );
    if (ret != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void PsramAudioCache::requestPreload(const Profiles::SoundFont& font) {
    PreloadJob job{0, font};
    uint32_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(m_jobMutex);
        generation = m_requestedGeneration.load(std::memory_order_relaxed) + 1;
        job.generation = generation;
        m_pendingJob = std::move(job);
        m_requestedGeneration.store(generation, std::memory_order_release);
    }
    ESP_LOGI(TAG, "Preload gen %lu requested for '%s'", static_cast<unsigned long>(generation),
             font.root().c_str());
    if (m_loaderTask) {
        xTaskNotifyGive(m_loaderTask);
    }
}

bool PsramAudioCache::isPreloadComplete() const {
    return m_completedGeneration.load(std::memory_order_acquire) ==
           m_requestedGeneration.load(std::memory_order_acquire);
}

uint8_t PsramAudioCache::loadedSwingPairCount() const {
    return m_loadedSwingPairs.load(std::memory_order_acquire);
}

std::string PsramAudioCache::humPath() {
    return mountedPath(kHumName);
}

std::string PsramAudioCache::swingLowPath(uint8_t pairIndex) {
    return mountedPath(swingLowName(pairIndex));
}

std::string PsramAudioCache::swingHighPath(uint8_t pairIndex) {
    return mountedPath(swingHighName(pairIndex));
}

void PsramAudioCache::loaderTaskFn(void* pvParameters) {
    static_cast<PsramAudioCache*>(pvParameters)->loaderLoop();
}

void PsramAudioCache::loaderLoop() {
    bool stackLogged = false;
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while (auto job = takePendingJob()) {
            runPreload(*job);
            if (!stackLogged) {
                stackLogged = true;
                ESP_LOGD(TAG, "Loader stack high-water mark: %u bytes",
                         static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
            }
        }
    }
}

std::optional<PsramAudioCache::PreloadJob> PsramAudioCache::takePendingJob() {
    std::lock_guard<std::mutex> lock(m_jobMutex);
    std::optional<PreloadJob> job = std::move(m_pendingJob);
    m_pendingJob.reset();
    return job;
}

bool PsramAudioCache::isSuperseded(uint32_t generation) const {
    return m_requestedGeneration.load(std::memory_order_acquire) != generation;
}

void PsramAudioCache::waitForDescriptorsClosed() {
    for (uint32_t waitedMs = 0; waitedMs < kCloseWaitMs; waitedMs += kClosePollMs) {
        if (m_vfs.openDescriptorCount() == 0) return;
        vTaskDelay(pdMS_TO_TICKS(kClosePollMs));
    }
    const uint8_t stillOpen = m_vfs.openDescriptorCount();
    if (stillOpen > 0) {
        ESP_LOGW(TAG, "%u /mem descriptor(s) still open after %lu ms; their buffers stay allocated",
                 stillOpen, static_cast<unsigned long>(kCloseWaitMs));
    }
}

void PsramAudioCache::runPreload(const PreloadJob& job) {
    configASSERT(xTaskGetCurrentTaskHandle() == m_loaderTask);
    const auto generation = static_cast<unsigned long>(job.generation);
    ESP_LOGI(TAG, "Starting PSRAM preload gen %lu for profile: %s", generation, job.font.root().c_str());

    m_loadedSwingPairs.store(0, std::memory_order_release);
    unloadAll();
    waitForDescriptorsClosed();

    if (isSuperseded(job.generation)) {
        ESP_LOGI(TAG, "Preload gen %lu superseded before loading", generation);
        return;
    }

    if (loadFile(job.font.humPath(), std::string(kHumName)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load hum.wav to PSRAM. Aborting preload.");
        m_completedGeneration.store(job.generation, std::memory_order_release);
        return;
    }
    ESP_LOGI(TAG, "Loaded to PSRAM: hum.wav");

    const uint8_t totalPairs = job.font.swingPairCount();
    for (uint8_t i = 1; i <= totalPairs; ++i) {
        if (isSuperseded(job.generation)) {
            ESP_LOGI(TAG, "Preload gen %lu superseded at pair %u", generation, i);
            return;
        }

        const std::string lowName = swingLowName(i);
        esp_err_t err = loadFile(job.font.swingLowPath(i), lowName);
        if (err == ESP_OK) {
            err = loadFile(job.font.swingHighPath(i), swingHighName(i));
            if (err != ESP_OK) {
                unloadFile(lowName);
            }
        }
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Preload stopped at pair %u (err=%s)", i, esp_err_to_name(err));
            break;
        }
        m_loadedSwingPairs.store(i, std::memory_order_release);
        ESP_LOGI(TAG, "Loaded to PSRAM: swing pair %u", i);
    }

    if (isSuperseded(job.generation)) {
        ESP_LOGI(TAG, "Preload gen %lu superseded after loading", generation);
        return;
    }

    m_completedGeneration.store(job.generation, std::memory_order_release);
    ESP_LOGI(TAG, "Preload gen %lu complete (pairs=%u). Free PSRAM: %zu bytes", generation,
             m_loadedSwingPairs.load(std::memory_order_relaxed),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

esp_err_t PsramAudioCache::loadFile(const std::string& sdPath, const std::string& vfsName) {
    configASSERT(xTaskGetCurrentTaskHandle() == m_loaderTask);

    std::unique_ptr<FILE, FileCloser> source(fopen(sdPath.c_str(), "rb"));
    if (!source) {
        ESP_LOGE(TAG, "Failed to open source file '%s'", sdPath.c_str());
        return ESP_ERR_NOT_FOUND;
    }

    long end = -1;
    if (fseek(source.get(), 0, SEEK_END) == 0) {
        end = ftell(source.get());
    }
    if (end < 0 || fseek(source.get(), 0, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Failed to determine size of '%s'", sdPath.c_str());
        return ESP_FAIL;
    }

    const auto size = static_cast<size_t>(end);
    if (size == 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) < size + kPsramHeadroomBytes) {
        ESP_LOGW(TAG, "Not enough PSRAM for '%s' (requires %zu + 256KB threshold)", vfsName.c_str(), size);
        return ESP_ERR_NO_MEM;
    }

    MemoryFile file;
    file.bytes.reset(static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    if (!file.bytes) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes in PSRAM for file '%s'", size, vfsName.c_str());
        return ESP_ERR_NO_MEM;
    }
    file.size = size;

    const size_t readBytes = fread(file.bytes.get(), 1, size, source.get());
    source.reset();
    if (readBytes != size) {
        ESP_LOGE(TAG, "Read size mismatch for '%s' (read %zu/%zu)", sdPath.c_str(), readBytes, size);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = m_vfs.registerFile(vfsName, std::make_shared<const MemoryFile>(std::move(file)));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register '%s' (err=%s)", vfsName.c_str(), esp_err_to_name(err));
        return err;
    }
    m_registeredNames.push_back(vfsName);

    ESP_LOGI(TAG, "Preloaded '%s' to PSRAM (%zu bytes). Free PSRAM: %zu bytes", vfsName.c_str(), size,
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return ESP_OK;
}

void PsramAudioCache::unloadFile(const std::string& vfsName) {
    configASSERT(xTaskGetCurrentTaskHandle() == m_loaderTask);
    auto it = std::find(m_registeredNames.begin(), m_registeredNames.end(), vfsName);
    if (it == m_registeredNames.end()) return;
    m_vfs.unregisterFile(vfsName);
    m_registeredNames.erase(it);
}

void PsramAudioCache::unloadAll() {
    configASSERT(xTaskGetCurrentTaskHandle() == m_loaderTask);
    for (const auto& name : m_registeredNames) {
        m_vfs.unregisterFile(name);
    }
    m_registeredNames.clear();
}

} // namespace InertialSaber::System
