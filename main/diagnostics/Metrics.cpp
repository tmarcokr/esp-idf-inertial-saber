// SPDX-License-Identifier: GPL-3.0-or-later

#include "diagnostics/Metrics.hpp"

#if CONFIG_SABER_METRICS

#include "esp_attr.h"
#include "esp_cpu.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#include <algorithm>
#include <atomic>
#include <limits>

namespace InertialSaber::Diagnostics {

namespace {

using Counter32 = std::atomic<uint32_t>;

static_assert(sizeof(Counter32) == sizeof(uint32_t));
static_assert(sizeof(std::atomic<TaskHandle_t>) == sizeof(uint32_t));
#if CONFIG_STDATOMIC_S32C1I_SPIRAM_WORKAROUND
// Warning: with PSRAM, IDF atomics are lock-free only in internal RAM; s_live must stay there.
#else
static_assert(Counter32::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<int32_t>::is_always_lock_free);
static_assert(std::atomic<TaskHandle_t>::is_always_lock_free);
#endif

struct LiveDuration {
    Counter32 count{0};
    Counter32 sumUs{0};
    Counter32 maxUs{0};
    Counter32 allocations{0};
    Counter32 scopesWithAllocations{0};
};

struct LiveBoot {
    Counter32 durationMs{0};
    Counter32 mainStackFreeMin{0};
    Counter32 mainPriority{0};
    Counter32 mainCore{0};
    Counter32 heapInternalFree{0};
    Counter32 heapPsramFree{0};
};

constexpr uint32_t kNoMark = 0;
constexpr int32_t kImuSettleUnknown = std::numeric_limits<int32_t>::min();

struct LiveStore {
    std::array<LiveDuration, kMetricCount> durations{};
    std::array<Counter32, kMetricCount> lastMarkUs{};
    std::array<Counter32, kBusCycleBucketCount> busCycleHistogram{};
    std::array<Counter32, kCounterCount> counters{};
    std::array<Counter32, kCoreCount> coreAllocations{};
    std::array<std::atomic<TaskHandle_t>, kTaskCount> taskHandles{};
    LiveBoot boot{};
    std::atomic<int32_t> imuSettleMs{kImuSettleUnknown};
    std::atomic<bool> sessionActive{false};
};

constinit LiveStore s_live{};

constexpr size_t index(Metric metric) {
    return static_cast<size_t>(metric);
}

uint32_t nowUs() {
    return static_cast<uint32_t>(esp_timer_get_time());
}

uint8_t currentCore() {
    return static_cast<uint8_t>(esp_cpu_get_core_id());
}

void updateMax(Counter32& target, uint32_t value) {
    uint32_t current = target.load(std::memory_order_relaxed);
    while (value > current &&
           !target.compare_exchange_weak(current, value, std::memory_order_relaxed)) {
    }
}

size_t busCycleBucket(uint32_t us) {
    const auto edge =
        std::upper_bound(kBusCycleBucketEdgesUs.begin(), kBusCycleBucketEdgesUs.end(), us);
    return static_cast<size_t>(edge - kBusCycleBucketEdgesUs.begin());
}

void notifyReporter(uint32_t bit) {
    if (TaskHandle_t reporter = Metrics::taskHandle(TaskId::Reporter); reporter != nullptr) {
        xTaskNotify(reporter, bit, eSetBits);
    }
}

} // namespace

void Metrics::recordDuration(Metric metric, uint32_t us, uint32_t allocations) {
    if (metric >= Metric::Count) return;

    LiveDuration& stats = s_live.durations[index(metric)];
    stats.count.fetch_add(1, std::memory_order_relaxed);
    stats.sumUs.fetch_add(us, std::memory_order_relaxed);
    updateMax(stats.maxUs, us);
    if (allocations > 0) {
        stats.allocations.fetch_add(allocations, std::memory_order_relaxed);
        stats.scopesWithAllocations.fetch_add(1, std::memory_order_relaxed);
    }
    if (metric == Metric::BusCycle) {
        s_live.busCycleHistogram[busCycleBucket(us)].fetch_add(1, std::memory_order_relaxed);
    }
}

void Metrics::markInterval(Metric metric) {
    if (metric >= Metric::Count) return;

    uint32_t now = nowUs();
    if (now == kNoMark) now = 1;
    const uint32_t previous =
        s_live.lastMarkUs[index(metric)].exchange(now, std::memory_order_relaxed);
    if (previous != kNoMark) {
        recordDuration(metric, now - previous, 0);
    }
}

void Metrics::increment(Counter counter, uint32_t n) {
    if (counter >= Counter::Count) return;
    s_live.counters[static_cast<size_t>(counter)].fetch_add(n, std::memory_order_relaxed);
}

void Metrics::registerTask(TaskId id, TaskHandle_t handle) {
    if (id >= TaskId::Count) return;
    s_live.taskHandles[static_cast<size_t>(id)].store(handle, std::memory_order_release);
}

TaskHandle_t Metrics::taskHandle(TaskId id) {
    if (id >= TaskId::Count) return nullptr;
    return s_live.taskHandles[static_cast<size_t>(id)].load(std::memory_order_acquire);
}

void Metrics::recordBoot() {
    s_live.boot.durationMs.store(static_cast<uint32_t>(esp_timer_get_time() / 1000),
                                 std::memory_order_relaxed);
    s_live.boot.mainStackFreeMin.store(static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr)),
                                       std::memory_order_relaxed);
    s_live.boot.mainPriority.store(static_cast<uint32_t>(uxTaskPriorityGet(nullptr)),
                                   std::memory_order_relaxed);
    s_live.boot.mainCore.store(static_cast<uint32_t>(xTaskGetCoreID(nullptr)),
                               std::memory_order_relaxed);
    s_live.boot.heapInternalFree.store(
        static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
        std::memory_order_relaxed);
    s_live.boot.heapPsramFree.store(
        static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
        std::memory_order_release);
}

Metrics::BootRecord Metrics::bootRecord() {
    return {
        .durationMs = s_live.boot.durationMs.load(std::memory_order_acquire),
        .mainStackFreeMin = s_live.boot.mainStackFreeMin.load(std::memory_order_relaxed),
        .mainPriority =
            static_cast<UBaseType_t>(s_live.boot.mainPriority.load(std::memory_order_relaxed)),
        .mainCore = static_cast<BaseType_t>(s_live.boot.mainCore.load(std::memory_order_relaxed)),
        .heapInternalFree = s_live.boot.heapInternalFree.load(std::memory_order_relaxed),
        .heapPsramFree = s_live.boot.heapPsramFree.load(std::memory_order_relaxed),
    };
}

void Metrics::recordImuSettle(int32_t settleMs) {
    s_live.imuSettleMs.store(settleMs, std::memory_order_relaxed);
}

std::optional<int32_t> Metrics::imuSettleMs() {
    const int32_t settleMs = s_live.imuSettleMs.load(std::memory_order_relaxed);
    if (settleMs == kImuSettleUnknown) return std::nullopt;
    return settleMs;
}

void Metrics::beginSession() {
    s_live.sessionActive.store(true, std::memory_order_release);
    notifyReporter(kSessionBeginBit);
}

void Metrics::endSession() {
    s_live.sessionActive.store(false, std::memory_order_release);
    notifyReporter(kSessionEndBit);
}

bool Metrics::sessionActive() {
    return s_live.sessionActive.load(std::memory_order_acquire);
}

uint32_t Metrics::peekCounter(Counter counter) {
    if (counter >= Counter::Count) return 0;
    return s_live.counters[static_cast<size_t>(counter)].load(std::memory_order_relaxed);
}

uint32_t Metrics::coreAllocations(uint8_t core) {
    if (core >= kCoreCount) return 0;
    return s_live.coreAllocations[core].load(std::memory_order_relaxed);
}

void Metrics::snapshotAndReset(Snapshot& out) {
    for (size_t i = 0; i < kMetricCount; ++i) {
        LiveDuration& live = s_live.durations[i];
        out.durations[i] = {
            .count = live.count.exchange(0, std::memory_order_relaxed),
            .sumUs = live.sumUs.exchange(0, std::memory_order_relaxed),
            .maxUs = live.maxUs.exchange(0, std::memory_order_relaxed),
            .allocations = live.allocations.exchange(0, std::memory_order_relaxed),
            .scopesWithAllocations =
                live.scopesWithAllocations.exchange(0, std::memory_order_relaxed),
        };
    }
    for (size_t i = 0; i < kBusCycleBucketCount; ++i) {
        out.busCycleHistogram[i] =
            s_live.busCycleHistogram[i].exchange(0, std::memory_order_relaxed);
    }
    for (size_t i = 0; i < kCounterCount; ++i) {
        out.counters[i] = s_live.counters[i].exchange(0, std::memory_order_relaxed);
    }
}

// Warning: called from inside every heap allocation; everything it reaches must stay in IRAM.
IRAM_ATTR void Metrics::noteAllocation() {
    const int core = esp_cpu_get_core_id();
    if (core >= 0 && static_cast<size_t>(core) < kCoreCount) {
        s_live.coreAllocations[static_cast<size_t>(core)].fetch_add(1, std::memory_order_relaxed);
    }
}

ScopeTimer::ScopeTimer(Metric metric)
    : m_metric(metric)
    , m_core(currentCore())
    , m_startUs(nowUs())
    , m_startAllocations(Metrics::coreAllocations(m_core)) {}

ScopeTimer::~ScopeTimer() {
    const uint32_t allocations = Metrics::coreAllocations(m_core) - m_startAllocations;
    Metrics::recordDuration(m_metric, nowUs() - m_startUs, allocations);
}

} // namespace InertialSaber::Diagnostics

#endif
