#pragma once

#include "system/MemoryVfs.hpp"
#include "profiles/SoundFont.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace InertialSaber::System {

/** @brief Preloads a sound font's hum and swing pairs from SD into PSRAM and serves them under /mem. */
class PsramAudioCache {
public:
    static constexpr std::string_view kMountPoint = "/mem";

    explicit PsramAudioCache(uint8_t maxFiles = 40, uint8_t maxFds = 8);
    ~PsramAudioCache();

    PsramAudioCache(const PsramAudioCache&) = delete;
    PsramAudioCache& operator=(const PsramAudioCache&) = delete;

    [[nodiscard]] esp_err_t init();

    /** @brief Queues a preload of @p font; callable from any task, supersedes any pending request. */
    void requestPreload(const Profiles::SoundFont& font);

    /** @brief True when the latest requested generation has finished loading. */
    [[nodiscard]] bool isPreloadComplete() const;

    [[nodiscard]] uint8_t loadedSwingPairCount() const;

    /** @brief /mem path of the hum file. */
    [[nodiscard]] static std::string humPath();
    /** @brief /mem path of the low swing of the 1-based @p pairIndex. */
    [[nodiscard]] static std::string swingLowPath(uint8_t pairIndex);
    /** @brief /mem path of the high swing of the 1-based @p pairIndex. */
    [[nodiscard]] static std::string swingHighPath(uint8_t pairIndex);

private:
    struct PreloadJob {
        uint32_t generation;
        Profiles::SoundFont font;
    };

    static constexpr uint32_t kCloseWaitMs = 200;
    static constexpr uint32_t kClosePollMs = 10;
    static constexpr size_t kPsramHeadroomBytes = 256 * 1024;

    static void loaderTaskFn(void* pvParameters);
    [[noreturn]] void loaderLoop();
    std::optional<PreloadJob> takePendingJob();
    void runPreload(const PreloadJob& job);
    [[nodiscard]] bool isSuperseded(uint32_t generation) const;
    void waitForDescriptorsClosed();

    [[nodiscard]] esp_err_t loadFile(const std::string& sdPath, const std::string& vfsName);
    void unloadFile(const std::string& vfsName);
    void unloadAll();

    Espressif::Wrappers::MemoryVfs m_vfs;
    TaskHandle_t m_loaderTask = nullptr;

    std::mutex m_jobMutex;
    std::optional<PreloadJob> m_pendingJob;
    std::atomic<uint32_t> m_requestedGeneration{0};
    std::atomic<uint32_t> m_completedGeneration{0};
    std::atomic<uint8_t> m_loadedSwingPairs{0};
    std::vector<std::string> m_registeredNames;
};

} // namespace InertialSaber::System
