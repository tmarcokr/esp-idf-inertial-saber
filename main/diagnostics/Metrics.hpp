#pragma once

#include "sdkconfig.h"

#include <cstddef>
#include <cstdint>

#if CONFIG_SABER_METRICS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#endif

namespace InertialSaber::Diagnostics {

/** @brief Timed quantities, recorded in microseconds. Reports list them in declaration order. */
enum class Metric : uint8_t {
    BusCycle,
    BusInterval,
    RunPreloadWait,
    RunSwing,
    RunLight,
    RunPowerToggle,
    RunBlaster,
    RunClash,
    RunDrag,
    RunProfileCycle,
    SwingActivate,
    SwingSwap,
    ImuRead,
    AudioPlayCall,
    AudioLatency,
    MotionAge,
    Count
};

/** @brief Event counters. Reports list them in declaration order. */
enum class Counter : uint8_t {
    BusTimeoutWakes,
    InputEventsDropped,
    ImuSamples,
    ImuEmptyReads,
    ImuPollTimeouts,
    OverlaysDropped,
    BusCycles,
    AudioCommandsDropped,
    AudioPlayFailed,
    InertialBursts,
    ClashDetections,
    ClashRetriggerLt1s,
    Count
};

/** @brief Tasks whose stack high-water mark is reported. Reports list them in declaration order. */
enum class TaskId : uint8_t {
    Main,
    Bus,
    Imu,
    PsramLoader,
    Reporter,
    SmartLed,
    ButtonPoll,
    EspTimer,
    AudioMixer,
    AudioSdReader,
    AudioMemReader,
    AudioControl,
    ProfileStore,
    ProfileControl,
    Count
};

inline constexpr size_t kMetricCount = static_cast<size_t>(Metric::Count);
inline constexpr size_t kCounterCount = static_cast<size_t>(Counter::Count);
inline constexpr size_t kTaskCount = static_cast<size_t>(TaskId::Count);

#if CONFIG_SABER_METRICS

/** @brief Exclusive upper edges (µs) of the BusCycle histogram buckets; the last one is open. */
inline constexpr std::array<uint32_t, 8> kBusCycleBucketEdgesUs{250,  500,   1000,  2000,
                                                                5000, 10000, 20000, 50000};
inline constexpr size_t kBusCycleBucketCount = kBusCycleBucketEdgesUs.size() + 1;
inline constexpr size_t kCoreCount = 2;

/**
 * @brief Static, lock-free recording facade for real-time metrics.
 *
 * All live state is preallocated in internal RAM; recording never allocates, locks or blocks and
 * is safe from any task. Allocation counts are attributed per CPU core, so a scope's allocation
 * delta includes allocations by other tasks that ran on the same core while it was open.
 */
class Metrics {
public:
    /** @brief Notification bit sent to the reporter task when an ignition starts. */
    static constexpr uint32_t kSessionBeginBit = 1U << 0;
    /** @brief Notification bit sent to the reporter task when a retraction completes. */
    static constexpr uint32_t kSessionEndBit = 1U << 1;

    /** @brief Accumulated statistics of one Metric. */
    struct DurationStats {
        uint32_t count;
        uint32_t sumUs;
        uint32_t maxUs;
        uint32_t allocations;
        uint32_t scopesWithAllocations;
    };

    /** @brief Values of all live accumulators taken by snapshotAndReset(). */
    struct Snapshot {
        std::array<DurationStats, kMetricCount> durations;
        std::array<uint32_t, kBusCycleBucketCount> busCycleHistogram;
        std::array<uint32_t, kCounterCount> counters;
    };

    /** @brief Values captured once at the end of the boot sequence. */
    struct BootRecord {
        uint32_t durationMs;
        uint32_t mainStackFreeMin;
        UBaseType_t mainPriority;
        BaseType_t mainCore;
        uint32_t heapInternalFree;
        uint32_t heapPsramFree;
    };

    Metrics() = delete;

    /** @brief Adds a sample of @p us to @p metric, with the heap @p allocations made during it. */
    static void recordDuration(Metric metric, uint32_t us, uint32_t allocations);

    /** @brief Records the time since the previous mark of @p metric; one writer per metric. */
    static void markInterval(Metric metric);

    static void increment(Counter counter, uint32_t n = 1);

    /** @brief Publishes @p handle so the reporter can read its stack high-water mark. */
    static void registerTask(TaskId id, TaskHandle_t handle);

    [[nodiscard]] static TaskHandle_t taskHandle(TaskId id);

    /**
     * @brief Captures the main task's stack high-water mark, priority and core, the boot duration
     * and free heap.
     */
    static void recordBoot();

    [[nodiscard]] static BootRecord bootRecord();

    /** @brief Marks an ignition; notifies the reporter task without blocking. */
    static void beginSession();

    /** @brief Marks a completed retraction; notifies the reporter task without blocking. */
    static void endSession();

    [[nodiscard]] static bool sessionActive();

    /** @brief Current value of @p counter, left unchanged. */
    [[nodiscard]] static uint32_t peekCounter(Counter counter);

    /** @brief Heap allocations on @p core since boot; monotonic, never reset, wraps at 2^32. */
    [[nodiscard]] static uint32_t coreAllocations(uint8_t core);

    /** @brief Moves the duration, histogram and counter accumulators into @p out, zeroing them. */
    static void snapshotAndReset(Snapshot& out);

    /** @brief Counts one heap allocation on the calling core; called by the allocation hook. */
    static void noteAllocation();
};

/** @brief Records the lifetime of the enclosing scope and the allocations made on its core. */
class ScopeTimer {
public:
    explicit ScopeTimer(Metric metric);
    ~ScopeTimer();

    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;

private:
    Metric m_metric;
    uint8_t m_core;
    uint32_t m_startUs;
    uint32_t m_startAllocations;
};

#endif

} // namespace InertialSaber::Diagnostics

#define SABER_METRIC_CONCAT_INNER(a, b) a##b
#define SABER_METRIC_CONCAT(a, b) SABER_METRIC_CONCAT_INNER(a, b)
#define SABER_METRIC_UNIQUE(prefix) SABER_METRIC_CONCAT(prefix, __LINE__)

#if CONFIG_SABER_METRICS
#define SABER_METRIC_SCOPE(metric)                                                                 \
    const ::InertialSaber::Diagnostics::ScopeTimer SABER_METRIC_UNIQUE(saberMetricScope_) {        \
        metric                                                                                     \
    }
#define SABER_METRIC_DURATION(metric, us)                                                          \
    ::InertialSaber::Diagnostics::Metrics::recordDuration((metric), (us), 0)
#define SABER_METRIC_INTERVAL(metric) ::InertialSaber::Diagnostics::Metrics::markInterval(metric)
#define SABER_METRIC_COUNT(counter) ::InertialSaber::Diagnostics::Metrics::increment(counter)
#define SABER_METRIC_ADD(counter, n)                                                               \
    ::InertialSaber::Diagnostics::Metrics::increment((counter), (n))
#define SABER_METRIC_REGISTER_TASK(taskId, handle)                                                 \
    ::InertialSaber::Diagnostics::Metrics::registerTask((taskId), (handle))
#define SABER_METRIC_RECORD_BOOT() ::InertialSaber::Diagnostics::Metrics::recordBoot()
#define SABER_METRIC_SESSION_BEGIN() ::InertialSaber::Diagnostics::Metrics::beginSession()
#define SABER_METRIC_SESSION_END() ::InertialSaber::Diagnostics::Metrics::endSession()
#else
#define SABER_METRIC_SCOPE(metric) static_cast<void>(0)
#define SABER_METRIC_DURATION(metric, us) static_cast<void>(0)
#define SABER_METRIC_INTERVAL(metric) static_cast<void>(0)
#define SABER_METRIC_COUNT(counter) static_cast<void>(0)
#define SABER_METRIC_ADD(counter, n) static_cast<void>(0)
#define SABER_METRIC_REGISTER_TASK(taskId, handle) static_cast<void>(0)
#define SABER_METRIC_RECORD_BOOT() static_cast<void>(0)
#define SABER_METRIC_SESSION_BEGIN() static_cast<void>(0)
#define SABER_METRIC_SESSION_END() static_cast<void>(0)
#endif
