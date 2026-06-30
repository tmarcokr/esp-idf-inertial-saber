#pragma once

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32S3

#include "system/MemoryVfs.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <atomic>
#include <cstdint>
#include <string>

namespace InertialSaber::System {

/**
 * @brief Manages loading SD card audio files into PSRAM and exposing them via MemoryVfs.
 */
class PsramAudioCache {
public:
    explicit PsramAudioCache(const char* mount_point = "/mem",
                             uint8_t max_files = 40,
                             uint8_t max_fds = 8);
    ~PsramAudioCache();

    PsramAudioCache(const PsramAudioCache&) = delete;
    PsramAudioCache& operator=(const PsramAudioCache&) = delete;

    [[nodiscard]] esp_err_t init();
    [[nodiscard]] esp_err_t loadFile(const std::string& sdPath, const char* vfsName);
    void unloadFile(const char* vfsName);
    void unloadAll();

    [[nodiscard]] const char* mountPoint() const;

    void requestProfilePreload(const std::string& profileRoot, uint8_t totalSwingPairs);
    [[nodiscard]] bool isPreloadComplete() const;
    [[nodiscard]] uint8_t getLoadedSwingPairCount() const;

private:
    struct CacheEntry {
        std::string name;
        uint8_t* buffer = nullptr;
        size_t size = 0;
        bool occupied = false;
    };

    struct PreloadRequest {
        char profileRoot[64];
        uint8_t totalSwingPairs;
    };

    static void loaderTaskFn(void* pvParameters);
    void runPreload(const std::string& profileRoot, uint8_t totalSwingPairs);

    Espressif::Wrappers::MemoryVfs m_vfs;
    CacheEntry* m_entries = nullptr;
    uint8_t m_maxFiles;

    QueueHandle_t m_loadQueue = nullptr;
    TaskHandle_t m_loaderTask = nullptr;
    std::atomic<bool> m_preloadComplete{false};
    std::atomic<uint8_t> m_loadedSwingPairs{0};
};

} // namespace InertialSaber::System

#endif // CONFIG_IDF_TARGET_ESP32S3
