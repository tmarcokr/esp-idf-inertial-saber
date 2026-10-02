#pragma once

#include "system/MemoryVfs.hpp"
#include "system/audio/AudioPath.hpp"
#include "system/hardware/HardwareConfig.hpp"
#include "profiles/SoundFont.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
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

    enum class PreloadStatus : uint8_t { Pending, Ready, Failed };

    static constexpr uint8_t kDefaultMaxFiles = 40;
    /** @brief Swing pairs that fit in the default /mem file table next to hum.wav. */
    static constexpr uint8_t kMaxSwingPairs = (kDefaultMaxFiles - 1) / 2;

    /** @param task Creation parameters of the loader task. */
    explicit PsramAudioCache(const Hardware::TaskSpec& task, uint8_t maxFiles = kDefaultMaxFiles,
                             uint8_t maxFds = 8);
    ~PsramAudioCache();

    PsramAudioCache(const PsramAudioCache&) = delete;
    PsramAudioCache& operator=(const PsramAudioCache&) = delete;

    /**
     * @brief Mounts /mem, allocates the internal DMA-capable copy buffer and spawns the loader task.
     * @return ESP_OK; the MemoryVfs::init() error; ESP_ERR_NO_MEM if the copy buffer or the task
     *         cannot be created.
     */
    [[nodiscard]] esp_err_t init();

    /** @brief Queues a preload of @p font; callable from any task, supersedes any pending request. */
    void requestPreload(const Profiles::SoundFont& font);

    /** @brief Outcome of the latest requested generation; Failed means hum.wav could not be loaded. */
    [[nodiscard]] PreloadStatus preloadStatus() const;

    [[nodiscard]] uint8_t loadedSwingPairCount() const;

    /** @brief /mem path of the hum file. */
    [[nodiscard]] static AudioPath humPath();
    /** @brief /mem path of the low swing of the 1-based @p pairIndex. */
    [[nodiscard]] static AudioPath swingLowPath(uint8_t pairIndex);
    /** @brief /mem path of the high swing of the 1-based @p pairIndex. */
    [[nodiscard]] static AudioPath swingHighPath(uint8_t pairIndex);

private:
    struct PreloadJob {
        uint32_t generation;
        Profiles::SoundFont font;
    };

    enum class FileRole : uint8_t { Required, Optional };

    struct JobContext {
        uint32_t generation;
        uint32_t fitWaitLeftMs;
        size_t copiedBytes;
        uint32_t copyUs;
    };

    static constexpr uint32_t kNoGeneration = 0;
    static constexpr size_t kBounceBytes = 32 * 1024;
    static constexpr uint32_t kCloseWaitMs = 1000;
    static constexpr uint32_t kFitWaitMs = 3000;
    static constexpr uint32_t kClosePollMs = 10;
    static constexpr size_t kPsramHeadroomBytes = 256 * 1024;

    static void loaderTaskFn(void* pvParameters);
    [[noreturn]] void loaderLoop();
    std::optional<PreloadJob> takePendingJob();
    void runPreload(const PreloadJob& job);
    [[nodiscard]] bool isSuperseded(uint32_t generation) const;
    void waitForDescriptorsClosed();

    [[nodiscard]] esp_err_t loadFile(const AudioPath& sdPath, const std::string& vfsName,
                                     FileRole role, JobContext& context);
    [[nodiscard]] bool waitForFit(size_t bytes, JobContext& context);
    [[nodiscard]] esp_err_t copyChunked(std::FILE* source, Espressif::Wrappers::MemoryFile& target,
                                        uint32_t generation);
    void unloadFile(const std::string& vfsName);
    void releaseFile(const std::string& vfsName);
    void unloadAll();

    const Hardware::TaskSpec m_taskSpec;
    Espressif::Wrappers::MemoryVfs m_vfs;
    std::unique_ptr<uint8_t[], Espressif::Wrappers::MemoryFile::HeapCapsDeleter> m_bounce;
    TaskHandle_t m_loaderTask = nullptr;

    std::mutex m_jobMutex;
    std::optional<PreloadJob> m_pendingJob;
    std::atomic<uint32_t> m_requestedGeneration{kNoGeneration};
    std::atomic<uint32_t> m_completedGeneration{kNoGeneration};
    std::atomic<uint32_t> m_failedGeneration{kNoGeneration};
    std::atomic<uint8_t> m_loadedSwingPairs{0};
    std::vector<std::string> m_registeredNames;
};

} // namespace InertialSaber::System
