#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32S3

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
    unloadAll();
    delete[] m_entries;
}

esp_err_t PsramAudioCache::init() {
    return m_vfs.init();
}

esp_err_t PsramAudioCache::loadFile(const std::string& sdPath, const char* vfsName) {
    if (vfsName == nullptr) return ESP_ERR_INVALID_ARG;

    // Find free slot
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

    // Allocate buffer in SPIRAM
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

} // namespace InertialSaber::System

#endif // CONFIG_IDF_TARGET_ESP32S3
