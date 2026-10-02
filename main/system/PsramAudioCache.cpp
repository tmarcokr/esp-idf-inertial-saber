#include "system/PsramAudioCache.hpp"
#include "system/Raii.hpp"
#include "diagnostics/Metrics.hpp"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>

static constexpr const char* TAG = "PsramAudioCache";

namespace InertialSaber::System {

namespace {

using Espressif::Wrappers::MemoryFile;

constexpr std::string_view kHumName = "hum.wav";
constexpr std::string_view kSwingLowPrefix = "swingl";
constexpr std::string_view kSwingHighPrefix = "swingh";

std::string swingLowName(uint8_t pairIndex) {
    return std::string(kSwingLowPrefix) + std::to_string(pairIndex) + ".wav";
}

std::string swingHighName(uint8_t pairIndex) {
    return std::string(kSwingHighPrefix) + std::to_string(pairIndex) + ".wav";
}

size_t largestPsramBlock() {
    return heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
}

size_t freePsram() {
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

uint32_t elapsedUs(int64_t sinceUs) {
    return static_cast<uint32_t>(esp_timer_get_time() - sinceUs);
}

AudioPath mountedPath(std::string_view prefix, uint8_t pairIndex) {
    return AudioPath(PsramAudioCache::kMountPoint)
        .append("/")
        .append(prefix)
        .appendNumber(pairIndex)
        .append(".wav");
}

} // namespace

PsramAudioCache::PsramAudioCache(const Hardware::TaskSpec& task, uint8_t maxFiles, uint8_t maxFds)
    : m_taskSpec(task)
    , m_vfs(kMountPoint, maxFiles, maxFds) {
    m_registeredNames.reserve(maxFiles);
}

PsramAudioCache::~PsramAudioCache() {
    if (m_loaderTask) {
        vTaskDelete(m_loaderTask);
        m_loaderTask = nullptr;
    }
}

esp_err_t PsramAudioCache::init() {
    m_bounce.reset(static_cast<uint8_t*>(
        heap_caps_malloc(kBounceBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)));
    if (!m_bounce) {
        ESP_LOGE(TAG, "Failed to allocate the %zu B internal DMA copy buffer (largest block %zu B)",
                 kBounceBytes,
                 heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = m_vfs.init();
    if (err != ESP_OK) return err;

    BaseType_t ret = xTaskCreatePinnedToCore(&PsramAudioCache::loaderTaskFn, m_taskSpec.name,
                                             m_taskSpec.stackSize, this, m_taskSpec.priority,
                                             &m_loaderTask, m_taskSpec.core);
    if (ret != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    SABER_METRIC_REGISTER_TASK(Diagnostics::TaskId::PsramLoader, m_loaderTask);

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
    ESP_LOGD(TAG, "Preload gen %lu requested for '%s'", static_cast<unsigned long>(generation),
             font.root().c_str());
    if (m_loaderTask) {
        xTaskNotifyGive(m_loaderTask);
    }
}

PsramAudioCache::PreloadStatus PsramAudioCache::preloadStatus() const {
    const uint32_t requested = m_requestedGeneration.load(std::memory_order_acquire);
    const uint32_t completed = m_completedGeneration.load(std::memory_order_acquire);
    if (requested == kNoGeneration || completed != requested) return PreloadStatus::Pending;
    return m_failedGeneration.load(std::memory_order_acquire) == completed ? PreloadStatus::Failed
                                                                           : PreloadStatus::Ready;
}

uint8_t PsramAudioCache::loadedSwingPairCount() const {
    return m_loadedSwingPairs.load(std::memory_order_acquire);
}

AudioPath PsramAudioCache::humPath() {
    return AudioPath(kMountPoint).append("/").append(kHumName);
}

AudioPath PsramAudioCache::swingLowPath(uint8_t pairIndex) {
    return mountedPath(kSwingLowPrefix, pairIndex);
}

AudioPath PsramAudioCache::swingHighPath(uint8_t pairIndex) {
    return mountedPath(kSwingHighPrefix, pairIndex);
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
        if (m_vfs.openDescriptorCount() == 0) break;
        vTaskDelay(pdMS_TO_TICKS(kClosePollMs));
    }
    const unsigned stillOpen = m_vfs.openDescriptorCount();
    const size_t freeBytes = freePsram();
    const size_t largest = largestPsramBlock();
    if (stillOpen > 0) {
        ESP_LOGW(TAG,
                 "%u /mem descriptor(s) still open after %lu ms; their buffers stay allocated "
                 "(free PSRAM %zu B, largest %zu B)",
                 stillOpen, static_cast<unsigned long>(kCloseWaitMs), freeBytes, largest);
    } else {
        ESP_LOGD(TAG, "/mem released: free PSRAM %zu B, largest %zu B, 0 descriptors open",
                 freeBytes, largest);
    }
}

bool PsramAudioCache::waitForFit(size_t bytes, JobContext& context) {
    const size_t needed = bytes + kPsramHeadroomBytes;
    while (largestPsramBlock() < needed) {
        if (context.fitWaitLeftMs == 0 || m_vfs.openDescriptorCount() == 0 ||
            isSuperseded(context.generation)) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(kClosePollMs));
        context.fitWaitLeftMs -= std::min(context.fitWaitLeftMs, kClosePollMs);
    }
    return true;
}

esp_err_t PsramAudioCache::copyChunked(std::FILE* source, MemoryFile& target, uint32_t generation) {
    size_t copied = 0;
    while (copied < target.size) {
        if (isSuperseded(generation)) return ESP_ERR_INVALID_STATE;
        const size_t chunk = std::min(kBounceBytes, target.size - copied);
        const size_t read = std::fread(m_bounce.get(), 1, chunk, source);
        std::memcpy(target.bytes.get() + copied, m_bounce.get(), read);
        copied += read;
        if (read != chunk) return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

void PsramAudioCache::runPreload(const PreloadJob& job) {
    configASSERT(xTaskGetCurrentTaskHandle() == m_loaderTask);
    const auto generation = static_cast<unsigned long>(job.generation);
    ESP_LOGI(TAG, "Starting PSRAM preload gen %lu for profile: %s", generation,
             job.font.root().c_str());
    const int64_t jobStartUs = esp_timer_get_time();

    m_loadedSwingPairs.store(0, std::memory_order_release);
    unloadAll();
    waitForDescriptorsClosed();

    if (isSuperseded(job.generation)) {
        ESP_LOGD(TAG, "Preload gen %lu superseded before loading", generation);
        return;
    }

    JobContext context{job.generation, kFitWaitMs, 0, 0};
    if (const esp_err_t err =
            loadFile(job.font.humPath(), std::string(kHumName), FileRole::Required, context);
        err != ESP_OK) {
        if (isSuperseded(job.generation)) {
            ESP_LOGD(TAG, "Preload gen %lu superseded while loading hum.wav", generation);
            return;
        }
        ESP_LOGE(TAG, "Preload gen %lu failed: hum.wav could not be loaded to PSRAM (%s)",
                 generation, esp_err_to_name(err));
        // Warning: must be published before m_completedGeneration; preloadStatus() relies on this order.
        m_failedGeneration.store(job.generation, std::memory_order_release);
        m_completedGeneration.store(job.generation, std::memory_order_release);
        return;
    }
    ESP_LOGD(TAG, "Loaded to PSRAM: hum.wav");

    const uint8_t totalPairs = job.font.swingPairCount();
    for (uint8_t i = 1; i <= totalPairs; ++i) {
        if (isSuperseded(job.generation)) {
            ESP_LOGD(TAG, "Preload gen %lu superseded at pair %u", generation, i);
            return;
        }

        const std::string lowName = swingLowName(i);
        esp_err_t err = loadFile(job.font.swingLowPath(i), lowName, FileRole::Optional, context);
        if (err == ESP_OK) {
            err =
                loadFile(job.font.swingHighPath(i), swingHighName(i), FileRole::Optional, context);
            if (err != ESP_OK) {
                unloadFile(lowName);
            }
        }
        if (err != ESP_OK) {
            if (!isSuperseded(job.generation)) {
                ESP_LOGW(TAG, "Preload stopped at pair %u (err=%s)", i, esp_err_to_name(err));
            }
            break;
        }
        m_loadedSwingPairs.store(i, std::memory_order_release);
        ESP_LOGD(TAG, "Loaded to PSRAM: swing pair %u", i);
    }

    if (isSuperseded(job.generation)) {
        ESP_LOGD(TAG, "Preload gen %lu superseded after loading", generation);
        return;
    }

    m_completedGeneration.store(job.generation, std::memory_order_release);
    const uint32_t copyRateKBps =
        context.copyUs > 0 ? static_cast<uint32_t>(static_cast<uint64_t>(context.copiedBytes) *
                                                   1000U / context.copyUs)
                           : 0;
    ESP_LOGI(TAG,
             "Preload gen %lu complete: '%s' hum + %u/%u pair(s), %zu B in %" PRIu32 " ms (%" PRIu32
             " kB/s), job %" PRIu32 " ms, free PSRAM %zu B, largest %zu B",
             generation, job.font.root().c_str(),
             static_cast<unsigned>(m_loadedSwingPairs.load(std::memory_order_relaxed)),
             static_cast<unsigned>(totalPairs), context.copiedBytes, context.copyUs / 1000U,
             copyRateKBps, elapsedUs(jobStartUs) / 1000U, freePsram(), largestPsramBlock());
}

esp_err_t PsramAudioCache::loadFile(const AudioPath& sdPath, const std::string& vfsName,
                                    FileRole role, JobContext& context) {
    configASSERT(xTaskGetCurrentTaskHandle() == m_loaderTask);

    if (!sdPath.ok()) {
        ESP_LOGE(TAG, "Source path of '%s' exceeds %u characters", vfsName.c_str(),
                 static_cast<unsigned>(AudioPath::kMaxLength));
        return ESP_ERR_INVALID_SIZE;
    }

    UniqueFile source = openFile(sdPath.c_str(), "rb");
    if (!source) {
        ESP_LOGE(TAG, "Failed to open source file '%s'", sdPath.c_str());
        return ESP_ERR_NOT_FOUND;
    }
    // Warning: must precede any other I/O on the stream. Unbuffered, every fread() of a whole
    // bounce chunk goes from FATFS straight into the DMA-capable buffer, sector-aligned.
    if (std::setvbuf(source.get(), nullptr, _IONBF, 0) != 0) {
        ESP_LOGE(TAG, "Failed to disable buffering of '%s'", sdPath.c_str());
        return ESP_FAIL;
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

    if (!waitForFit(size, context)) {
        if (isSuperseded(context.generation)) return ESP_ERR_INVALID_STATE;
        const esp_log_level_t level = role == FileRole::Required ? ESP_LOG_ERROR : ESP_LOG_WARN;
        ESP_LOG_LEVEL_LOCAL(level, TAG,
                            "Not enough PSRAM for '%s': %zu B + %zu B headroom, largest block "
                            "%zu B, free %zu B, %u /mem descriptor(s) open",
                            vfsName.c_str(), size, kPsramHeadroomBytes, largestPsramBlock(),
                            freePsram(), static_cast<unsigned>(m_vfs.openDescriptorCount()));
        return ESP_ERR_NO_MEM;
    }

    MemoryFile file = MemoryFile::allocate(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!file.bytes) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes in PSRAM for file '%s'", size, vfsName.c_str());
        return ESP_ERR_NO_MEM;
    }

    const int64_t copyStartUs = esp_timer_get_time();
    const esp_err_t copyErr = copyChunked(source.get(), file, context.generation);
    const uint32_t copyUs = elapsedUs(copyStartUs);
    source.reset();
    if (copyErr != ESP_OK) {
        if (!isSuperseded(context.generation)) {
            ESP_LOGE(TAG, "Failed to copy '%s' to PSRAM (%s)", sdPath.c_str(),
                     esp_err_to_name(copyErr));
        }
        return copyErr;
    }
    SABER_METRIC_DURATION(Diagnostics::Metric::PreloadCopy, copyUs);
    SABER_METRIC_ADD(Diagnostics::Counter::PreloadBytes, static_cast<uint32_t>(size));
    context.copiedBytes += size;
    context.copyUs += copyUs;

    esp_err_t err =
        m_vfs.registerFile(vfsName, std::make_shared<const MemoryFile>(std::move(file)));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register '%s' (err=%s)", vfsName.c_str(), esp_err_to_name(err));
        return err;
    }
    m_registeredNames.push_back(vfsName);

    ESP_LOGD(TAG, "Preloaded '%s' to PSRAM (%zu B in %" PRIu32 " us). Free PSRAM: %zu B",
             vfsName.c_str(), size, copyUs, freePsram());
    return ESP_OK;
}

void PsramAudioCache::unloadFile(const std::string& vfsName) {
    configASSERT(xTaskGetCurrentTaskHandle() == m_loaderTask);
    auto it = std::find(m_registeredNames.begin(), m_registeredNames.end(), vfsName);
    if (it == m_registeredNames.end()) return;
    releaseFile(vfsName);
    m_registeredNames.erase(it);
}

void PsramAudioCache::releaseFile(const std::string& vfsName) {
    const esp_err_t err = m_vfs.unregisterFile(vfsName);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to unregister '%s' (err=%s)", vfsName.c_str(), esp_err_to_name(err));
    }
}

void PsramAudioCache::unloadAll() {
    configASSERT(xTaskGetCurrentTaskHandle() == m_loaderTask);
    for (const auto& name : m_registeredNames) {
        releaseFile(name);
    }
    m_registeredNames.clear();
}

} // namespace InertialSaber::System
