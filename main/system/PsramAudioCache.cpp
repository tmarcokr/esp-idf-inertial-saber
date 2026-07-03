#include "sdkconfig.h"


#include "system/PsramAudioCache.hpp"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <cstdio>

static constexpr const char* TAG = "PsramAudioCache";

namespace InertialSaber::System {

PsramAudioCache::PsramAudioCache(const char* mount_point, uint8_t max_files, uint8_t max_fds)
    : m_vfs(mount_point, max_files, max_fds)
    , m_maxFiles(max_files) {
    m_entries = new CacheEntry[m_maxFiles];
}

PsramAudioCache::~PsramAudioCache() {
    if (m_loaderTask) {
        vTaskDelete(m_loaderTask);
        m_loaderTask = nullptr;
    }
    if (m_loadQueue) {
        vQueueDelete(m_loadQueue);
        m_loadQueue = nullptr;
    }
    unloadAll();
    delete[] m_entries;
}

esp_err_t PsramAudioCache::init() {
    esp_err_t err = m_vfs.init();
    if (err != ESP_OK) return err;

    m_loadQueue = xQueueCreate(2, sizeof(PreloadRequest));
    if (!m_loadQueue) {
        return ESP_ERR_NO_MEM;
    }

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

esp_err_t PsramAudioCache::loadFile(const std::string& sdPath, const char* vfsName) {
    if (vfsName == nullptr) return ESP_ERR_INVALID_ARG;

    int slot = -1;
    for (uint8_t i = 0; i < m_maxFiles; ++i) {
        if (!m_entries[i].occupied) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        ESP_LOGE(TAG, "No free cache slots available for '%s'", vfsName);
        return ESP_ERR_NO_MEM;
    }

    FILE* f = fopen(sdPath.c_str(), "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open source file '%s'", sdPath.c_str());
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size == 0) {
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }

    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) < size + 256 * 1024) {
        ESP_LOGW(TAG, "Not enough PSRAM for '%s' (requires %zu + 256KB threshold)", vfsName, size);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    uint8_t* buffer = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buffer) {
        fclose(f);
        ESP_LOGE(TAG, "Failed to allocate %zu bytes in PSRAM for file '%s'", size, vfsName);
        return ESP_ERR_NO_MEM;
    }

    size_t readBytes = fread(buffer, 1, size, f);
    fclose(f);

    if (readBytes != size) {
        heap_caps_free(buffer);
        ESP_LOGE(TAG, "Read size mismatch for '%s' (read %zu/%zu)", sdPath.c_str(), readBytes, size);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = m_vfs.registerFile(vfsName, buffer, size);
    if (err != ESP_OK) {
        heap_caps_free(buffer);
        return err;
    }

    m_entries[slot].name = vfsName;
    m_entries[slot].buffer = buffer;
    m_entries[slot].size = size;
    m_entries[slot].occupied = true;

    size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "Preloaded '%s' to PSRAM (%zu bytes). Free PSRAM: %zu bytes", vfsName, size, freePsram);

    return ESP_OK;
}

void PsramAudioCache::unloadFile(const char* vfsName) {
    if (!vfsName) return;

    for (uint8_t i = 0; i < m_maxFiles; ++i) {
        if (m_entries[i].occupied && m_entries[i].name == vfsName) {
            m_vfs.unregisterFile(vfsName);
            heap_caps_free(m_entries[i].buffer);
            m_entries[i].occupied = false;
            m_entries[i].name.clear();
            m_entries[i].buffer = nullptr;
            m_entries[i].size = 0;
            break;
        }
    }
}

void PsramAudioCache::unloadAll() {
    for (uint8_t i = 0; i < m_maxFiles; ++i) {
        if (m_entries[i].occupied) {
            m_vfs.unregisterFile(m_entries[i].name.c_str());
            heap_caps_free(m_entries[i].buffer);
            m_entries[i].occupied = false;
            m_entries[i].name.clear();
            m_entries[i].buffer = nullptr;
            m_entries[i].size = 0;
        }
    }
}

const char* PsramAudioCache::mountPoint() const {
    return "/mem";
}

void PsramAudioCache::requestProfilePreload(const std::string& profileRoot, uint8_t totalSwingPairs) {
    m_preloadComplete.store(false);
    m_loadedSwingPairs.store(0);

    PreloadRequest req{};
    snprintf(req.profileRoot, sizeof(req.profileRoot), "%s", profileRoot.c_str());
    req.totalSwingPairs = totalSwingPairs;

    if (m_loadQueue) {
        PreloadRequest dummy;
        (void)xQueueReceive(m_loadQueue, &dummy, 0);
        xQueueSend(m_loadQueue, &req, 0);
    }
}

bool PsramAudioCache::isPreloadComplete() const {
    return m_preloadComplete.load();
}

uint8_t PsramAudioCache::getLoadedSwingPairCount() const {
    return m_loadedSwingPairs.load();
}

void PsramAudioCache::loaderTaskFn(void* pvParameters) {
    auto* self = static_cast<PsramAudioCache*>(pvParameters);
    PreloadRequest req;
    while (true) {
        if (xQueueReceive(self->m_loadQueue, &req, portMAX_DELAY) == pdTRUE) {
            self->runPreload(req.profileRoot, req.totalSwingPairs);
        }
    }
}

void PsramAudioCache::runPreload(const std::string& profileRoot, uint8_t totalSwingPairs) {
    ESP_LOGI(TAG, "Iniciando carga a PSRAM para profile: %s", profileRoot.c_str());
    unloadAll();

    std::string humSd = "/sdcard/" + profileRoot + "/hum.wav";
    if (loadFile(humSd, "hum.wav") != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load hum.wav to PSRAM. Aborting preload.");
        m_preloadComplete.store(true);
        return;
    }
    ESP_LOGI(TAG, "Cargado a PSRAM: hum.wav");

    for (uint8_t i = 1; i <= totalSwingPairs; ++i) {
        std::string suffix = std::to_string(i) + ".wav";
        std::string swlSd = "/sdcard/" + profileRoot + "/swingl/swingl" + suffix;
        std::string swhSd = "/sdcard/" + profileRoot + "/swingh/swingh" + suffix;
        std::string vfsL = "swingl" + std::to_string(i) + ".wav";
        std::string vfsH = "swingh" + std::to_string(i) + ".wav";

        if (loadFile(swlSd, vfsL.c_str()) != ESP_OK) {
            ESP_LOGW(TAG, "Memoria PSRAM llena. Carga detenida en el par %d", i);
            break;
        }
        if (loadFile(swhSd, vfsH.c_str()) != ESP_OK) {
            unloadFile(vfsL.c_str());
            ESP_LOGW(TAG, "Memoria PSRAM llena. Carga detenida en el par %d", i);
            break;
        }
        m_loadedSwingPairs.store(i);
        ESP_LOGI(TAG, "Cargado a PSRAM: par swing %d", i);
    }

    m_preloadComplete.store(true);
    ESP_LOGI(TAG, "Carga a PSRAM completada. Pares totales cargados: %d", m_loadedSwingPairs.load());
}

} // namespace InertialSaber::System

