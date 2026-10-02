#pragma once

#include "sdkconfig.h"

#if CONFIG_SABER_METRICS

#include "diagnostics/Metrics.hpp"
#include "system/PsramAudioCache.hpp"
#include "system/status/StatusIndicator.hpp"

#include "AudioEngine.hpp"

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#include <cstdint>
#include <memory>

namespace InertialSaber::System::Monitoring {

/**
 * @brief Low-priority task that aggregates the live metrics per ignition session and appends them
 * as CSV blocks to /sdcard/metrics/session_NNN.csv while the saber is retracted, the audio idle and
 * no PSRAM preload is reading the SD card.
 */
class MetricsReporter {
public:
    /**
     * @param audio Engine whose lock-free output level gates the SD writes.
     * @param audioCache Cache whose preload status gates the SD writes.
     * @param status Indicator that signals every write attempt and its outcome.
     */
    MetricsReporter(const Espressif::Wrappers::Audio::AudioEngine& audio,
                    const PsramAudioCache& audioCache, Status::StatusIndicator& status);
    ~MetricsReporter();

    MetricsReporter(const MetricsReporter&) = delete;
    MetricsReporter& operator=(const MetricsReporter&) = delete;

    /**
     * @brief Allocate the PSRAM buffers, resolve the component task handles and spawn the task.
     * @return ESP_OK; ESP_ERR_NO_MEM if a buffer or the task cannot be created;
     *         ESP_ERR_INVALID_STATE if already started.
     */
    [[nodiscard]] esp_err_t start();

private:
    struct RateStats {
        uint32_t minHz;
        uint32_t maxHz;
        uint32_t avgTenthsHz;
    };

    struct HeapStats {
        uint32_t internalStart;
        uint32_t internalMin;
        uint32_t internalEnd;
        uint32_t internalLargestMin;
        uint32_t internalMinSinceBoot;
        uint32_t psramStart;
        uint32_t psramMin;
        uint32_t psramEnd;
    };

    struct TaskReading {
        bool known;
        uint32_t stackFreeMin;
        UBaseType_t priority;
        BaseType_t core;
    };

    struct BlockSnapshot {
        uint32_t block;
        uint32_t windowMs;
        uint32_t onTimeMs;
        uint32_t lostBefore;
        RateStats busRate;
        RateStats imuRate;
        HeapStats heap;
        std::array<TaskReading, Diagnostics::kTaskCount> tasks;
        std::array<uint32_t, Diagnostics::kCoreCount> coreAllocations;
        Diagnostics::Metrics::Snapshot live;
    };

    struct RateWindow {
        uint32_t lastCount;
        uint32_t minHz;
        uint32_t maxHz;
        bool hasSample;
    };

    struct HeapCapsFree {
        void operator()(void* p) const { heap_caps_free(p); }
    };

    enum class WriteOutcome : uint8_t { Written, Dropped, Deferred };
    enum class FlushOutcome : uint8_t { Written, Failed, Postponed };

    class CsvWriter;

    static constexpr uint32_t kSampleIntervalMs = 100;
    static constexpr uint32_t kSignalRefreshMs = 50;
    static constexpr uint32_t kWrittenSignalMs = 1000;
    static constexpr uint32_t kWriteFailedSignalMs = 2000;
    static constexpr uint32_t kRateWindowMs = 1000;
    static constexpr uint32_t kFlushSettleMs = 2000;
    static constexpr uint32_t kAudioIdleHoldMs = 500;
    static constexpr size_t kMaxPendingBlocks = 4;
    static constexpr size_t kTextBufferBytes = 8192;
    static constexpr uint32_t kMaxFileIndex = 999;
    static constexpr size_t kDirectoryCapacity = 24;
    static constexpr size_t kFilePathCapacity = 48;

    static void taskEntry(void* arg);
    [[noreturn]] void run();

    void resolveComponentTasks();
    void handleNotifications(uint32_t bits, int64_t nowUs);
    void beginSession(int64_t nowUs);
    void endSession(int64_t nowUs);
    void sampleHeap();
    void updateRates(int64_t nowUs);
    void updateAudioIdle(int64_t nowUs);
    void resetBlockAccumulators(int64_t nowUs, uint32_t busCyclesBaseline,
                                uint32_t imuSamplesBaseline);
    void captureBlock(BlockSnapshot& block, int64_t nowUs);
    [[nodiscard]] bool flushDue(int64_t nowUs) const;
    void flush(int64_t nowUs);
    [[nodiscard]] FlushOutcome writePending();
    void startSignal(Status::ActivitySignal signal, int64_t nowUs);
    void updateSignal(int64_t nowUs);
    void warnSdUnavailable(const char* path);
    [[nodiscard]] bool resolveFilePath();
    [[nodiscard]] WriteOutcome writeBootBlock();
    [[nodiscard]] WriteOutcome writeSessionBlock(const BlockSnapshot& block);

    const Espressif::Wrappers::Audio::AudioEngine& m_audio;
    const PsramAudioCache& m_audioCache;
    Status::StatusIndicator& m_status;
    TaskHandle_t m_task = nullptr;

    Status::ActivitySignal m_signal = Status::ActivitySignal::None;
    int64_t m_signalEndUs = 0;

    std::unique_ptr<BlockSnapshot[], HeapCapsFree> m_pending;
    std::unique_ptr<char[], HeapCapsFree> m_text;
    size_t m_pendingHead = 0;
    size_t m_pendingCount = 0;

    bool m_sessionActive = false;
    bool m_bootBlockWritten = false;
    bool m_filePathResolved = false;
    bool m_sdWarningLogged = false;
    std::array<char, kFilePathCapacity> m_filePath{};

    uint32_t m_nextBlock = 1;
    uint32_t m_sessionsLost = 0;
    int64_t m_blockStartUs = 0;
    int64_t m_sessionStartUs = 0;
    int64_t m_onTimeUs = 0;
    int64_t m_lastEndUs = 0;
    int64_t m_nextFlushAttemptUs = 0;
    int64_t m_audioIdleSinceUs = 0;
    bool m_audioIdle = false;

    bool m_heapStartCaptured = false;
    HeapStats m_heap{};
    std::array<uint32_t, Diagnostics::kCoreCount> m_blockStartCoreAllocations{};

    int64_t m_rateWindowStartUs = 0;
    RateWindow m_busRate{};
    RateWindow m_imuRate{};
    uint32_t m_blockStartBusCycles = 0;
    uint32_t m_blockStartImuSamples = 0;
};

} // namespace InertialSaber::System::Monitoring

#endif
