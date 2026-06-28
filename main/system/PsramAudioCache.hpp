#pragma once

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32S3

#include "system/MemoryVfs.hpp"
#include "esp_err.h"
#include <cstdint>
#include <string>

namespace InertialSaber::System {

/**
 * @brief Manages loading SD card audio files into PSRAM and exposing them via MemoryVfs.
 */
class PsramAudioCache {
public:
    explicit PsramAudioCache(const char* mount_point = "/mem",
                             uint8_t max_files = 8,
                             uint8_t max_fds = 8);
    ~PsramAudioCache();

    PsramAudioCache(const PsramAudioCache&) = delete;
    PsramAudioCache& operator=(const PsramAudioCache&) = delete;

    [[nodiscard]] esp_err_t init();
    [[nodiscard]] esp_err_t loadFile(const std::string& sdPath, const char* vfsName);
    void unloadFile(const char* vfsName);
    void unloadAll();

    [[nodiscard]] const char* mountPoint() const;

private:
    struct CacheEntry {
        std::string name;
        uint8_t* buffer = nullptr;
        size_t size = 0;
        bool occupied = false;
    };

    Espressif::Wrappers::MemoryVfs m_vfs;
    CacheEntry* m_entries = nullptr;
    uint8_t m_maxFiles;
};

} // namespace InertialSaber::System

#endif // CONFIG_IDF_TARGET_ESP32S3
