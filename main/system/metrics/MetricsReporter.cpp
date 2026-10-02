#include "system/metrics/MetricsReporter.hpp"

#if CONFIG_SABER_METRICS

#include "profiles/ProfileManager.hpp"
#include "system/Raii.hpp"
#include "system/hardware/HardwareConfig.hpp"

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#include <sys/stat.h>

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cinttypes>
#include <climits>
#include <cstdarg>
#include <cstring>
#include <iterator>
#include <string_view>

namespace InertialSaber::System::Monitoring {

namespace {

constexpr const char* TAG = "MetricsReporter";

using Diagnostics::Counter;
using Diagnostics::Metric;
using Diagnostics::TaskId;

constexpr uint32_t kSchemaVersion = 1;
constexpr uint32_t kStackMarginBytes = 1024;
constexpr uint32_t kBusCycleBudgetUs = 2000;
constexpr int64_t kUsPerMs = 1000;
constexpr uint64_t kTenthsPerSecondUs = 10'000'000ULL;
constexpr std::string_view kFilePrefix = "session_";
constexpr std::string_view kFileSuffix = ".csv";

constexpr int32_t kQ15One = 1 << 15;
constexpr int32_t kMixerFullScaleSample = 32767;
constexpr int32_t kMixerFullScaleLevel = 16384;
constexpr uint16_t kAudioIdleLevelCeiling = 256;

constexpr uint16_t
dcBlockerSilentResidualLevel(Espressif::Wrappers::Audio::DcBlocker::CutoffPreset cutoff) {
    const int32_t maxStuckSample = (kQ15One - 1) / (kQ15One - static_cast<int32_t>(cutoff));
    return static_cast<uint16_t>(maxStuckSample * kMixerFullScaleLevel / kMixerFullScaleSample);
}

constexpr uint16_t kAudioIdleLevel =
    2 * dcBlockerSilentResidualLevel(Hardware::HardwareConfig::kAudioDcCutoff);
static_assert(kAudioIdleLevel <= kAudioIdleLevelCeiling);

enum class MetricKind : uint8_t { Scope, Duration, Interval };

struct MetricInfo {
    const char* name;
    MetricKind kind;
};

constexpr MetricInfo kMetricInfo[] = {
    {"bus_cycle", MetricKind::Scope},        {"bus_interval", MetricKind::Interval},
    {"run_preload_wait", MetricKind::Scope}, {"run_swing", MetricKind::Scope},
    {"run_light", MetricKind::Scope},        {"run_power_toggle", MetricKind::Scope},
    {"run_blaster", MetricKind::Scope},      {"run_clash", MetricKind::Scope},
    {"run_drag", MetricKind::Scope},         {"run_profile_cycle", MetricKind::Scope},
    {"swing_activate", MetricKind::Scope},   {"swing_swap", MetricKind::Scope},
    {"imu_read", MetricKind::Scope},         {"audio_play_call", MetricKind::Scope},
    {"audio_latency", MetricKind::Duration}, {"motion_age", MetricKind::Duration},
    {"profile_commit", MetricKind::Scope},   {"profile_build", MetricKind::Scope},
    {"profile_save", MetricKind::Scope},     {"profile_switch", MetricKind::Duration},
};
static_assert(std::size(kMetricInfo) == Diagnostics::kMetricCount);

constexpr const char* kCounterNames[] = {
    "bus_timeout_wakes", "input_events_dropped", "imu_samples",      "imu_empty_reads",
    "imu_poll_timeouts", "overlays_dropped",     "bus_cycles",       "audio_commands_dropped",
    "audio_play_failed", "inertial_bursts",      "clash_detections", "clash_retrigger_lt_1s",
};
static_assert(std::size(kCounterNames) == Diagnostics::kCounterCount);

using Hardware::TaskTable;

constexpr const Hardware::TaskSpec& kTask = TaskTable::kMetricsReporter;

struct TaskInfo {
    const char* name;
    const Hardware::TaskSpec& spec;
    bool resolveByName;
};

constexpr TaskInfo kTaskInfo[] = {
    {"main", TaskTable::kMain, false},
    {"saber_bus", TaskTable::kBus, false},
    {"imu_adapter", TaskTable::kImuAdapter, false},
    {"psram_loader", TaskTable::kPsramLoader, false},
    {"metrics", TaskTable::kMetricsReporter, false},
    {"SmartLedTask", TaskTable::kSmartLed, true},
    {"gpio_btn_tsk", TaskTable::kButtonPoll, true},
    {"esp_timer", TaskTable::kEspTimer, true},
    {"audio_mixer", TaskTable::kAudioMixer, true},
    {"audio_sd_reader", TaskTable::kAudioSdReader, true},
    {"audio_mem_reader", TaskTable::kAudioMemReader, true},
    {"audio_ctrl", TaskTable::kAudioControl, false},
    {"profile_ctrl", TaskTable::kProfileControl, false},
};
static_assert(std::size(kTaskInfo) == Diagnostics::kTaskCount);

constexpr const char* kBusCycleBucketNames[] = {
    "lt_250us", "lt_500us", "lt_1ms",  "lt_2ms",  "lt_5ms",
    "lt_10ms",  "lt_20ms",  "lt_50ms", "ge_50ms",
};
static_assert(std::size(kBusCycleBucketNames) == Diagnostics::kBusCycleBucketCount);

constexpr size_t kFirstRunMetric = static_cast<size_t>(Metric::RunPreloadWait);
constexpr size_t kLastRunMetric = static_cast<size_t>(Metric::RunProfileCycle);

constexpr size_t index(Metric metric) {
    return static_cast<size_t>(metric);
}

constexpr size_t index(Counter counter) {
    return static_cast<size_t>(counter);
}

constexpr const char* optimizationLevel() {
#if CONFIG_COMPILER_OPTIMIZATION_DEBUG
    return "debug";
#elif CONFIG_COMPILER_OPTIMIZATION_PERF
    return "perf";
#elif CONFIG_COMPILER_OPTIMIZATION_SIZE
    return "size";
#else
    return "none";
#endif
}

constexpr const char* resetReasonName(esp_reset_reason_t reason) {
    switch (reason) {
    case ESP_RST_UNKNOWN:
        return "UNKNOWN";
    case ESP_RST_POWERON:
        return "POWERON";
    case ESP_RST_EXT:
        return "EXT";
    case ESP_RST_SW:
        return "SW";
    case ESP_RST_PANIC:
        return "PANIC";
    case ESP_RST_INT_WDT:
        return "INT_WDT";
    case ESP_RST_TASK_WDT:
        return "TASK_WDT";
    case ESP_RST_WDT:
        return "WDT";
    case ESP_RST_DEEPSLEEP:
        return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
        return "BROWNOUT";
    case ESP_RST_SDIO:
        return "SDIO";
    case ESP_RST_USB:
        return "USB";
    case ESP_RST_JTAG:
        return "JTAG";
    case ESP_RST_EFUSE:
        return "EFUSE";
    case ESP_RST_PWR_GLITCH:
        return "PWR_GLITCH";
    case ESP_RST_CPU_LOCKUP:
        return "CPU_LOCKUP";
    default:
        return nullptr;
    }
}

constexpr const char* passFail(bool pass) {
    return pass ? "PASS" : "FAIL";
}

uint32_t toMs(int64_t us) {
    return static_cast<uint32_t>(us / kUsPerMs);
}

uint32_t internalFree() {
    return static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

uint32_t psramFree() {
    return static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

uint32_t internalLargestBlock() {
    return static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
}

bool parseFileIndex(std::string_view name, uint32_t& fileIndex) {
    if (!name.starts_with(kFilePrefix) || !name.ends_with(kFileSuffix)) return false;
    const std::string_view digits =
        name.substr(kFilePrefix.size(), name.size() - kFilePrefix.size() - kFileSuffix.size());
    if (digits.empty()) return false;
    const auto [end, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), fileIndex);
    return ec == std::errc{} && end == digits.data() + digits.size();
}

} // namespace

class MetricsReporter::CsvWriter {
public:
    CsvWriter(std::FILE* file, char* buffer, size_t capacity, uint32_t block)
        : m_file(file)
        , m_buffer(buffer)
        , m_capacity(capacity)
        , m_block(block) {}

    CsvWriter(const CsvWriter&) = delete;
    CsvWriter& operator=(const CsvWriter&) = delete;

    void line(const char* text) { append("%s\n", text); }

    void text(const char* section, const char* name, const char* stat, const char* value,
              const char* unit) {
        append("%" PRIu32 ",%s,%s,%s,%s,%s\n", m_block, section, name, stat, value, unit);
    }

    void number(const char* section, const char* name, const char* stat, uint32_t value,
                const char* unit) {
        append("%" PRIu32 ",%s,%s,%s,%" PRIu32 ",%s\n", m_block, section, name, stat, value, unit);
    }

    void tenths(const char* section, const char* name, const char* stat, uint32_t value,
                const char* unit) {
        append("%" PRIu32 ",%s,%s,%s,%" PRIu32 ".%" PRIu32 ",%s\n", m_block, section, name, stat,
               value / 10, value % 10, unit);
    }

    [[nodiscard]] bool finish() {
        drain();
        return m_ok;
    }

private:
    __attribute__((format(printf, 2, 3))) void append(const char* format, ...) {
        for (int attempt = 0; attempt < 2 && m_ok; ++attempt) {
            const size_t room = m_capacity - m_used;
            va_list args;
            va_start(args, format);
            const int written = std::vsnprintf(m_buffer + m_used, room, format, args);
            va_end(args);
            if (written < 0) {
                m_ok = false;
                return;
            }
            if (static_cast<size_t>(written) < room) {
                m_used += static_cast<size_t>(written);
                return;
            }
            drain();
        }
        m_ok = false;
    }

    void drain() {
        if (m_used == 0 || !m_ok) return;
        m_ok = std::fwrite(m_buffer, 1, m_used, m_file) == m_used;
        m_used = 0;
    }

    std::FILE* m_file;
    char* m_buffer;
    size_t m_capacity;
    uint32_t m_block;
    size_t m_used = 0;
    bool m_ok = true;
};

MetricsReporter::MetricsReporter(const Espressif::Wrappers::Audio::AudioEngine& audio,
                                 const PsramAudioCache& audioCache,
                                 const Profiles::ProfileManager& profiles,
                                 Status::StatusIndicator& status)
    : m_audio(audio)
    , m_audioCache(audioCache)
    , m_profiles(profiles)
    , m_status(status) {}

MetricsReporter::~MetricsReporter() {
    if (m_task != nullptr) {
        Diagnostics::Metrics::registerTask(TaskId::Reporter, nullptr);
        vTaskDelete(m_task);
        m_task = nullptr;
    }
}

esp_err_t MetricsReporter::start() {
    if (m_task != nullptr) return ESP_ERR_INVALID_STATE;

    m_pending.reset(static_cast<BlockSnapshot*>(
        heap_caps_calloc(kMaxPendingBlocks, sizeof(BlockSnapshot), MALLOC_CAP_SPIRAM)));
    m_text.reset(static_cast<char*>(heap_caps_calloc(kTextBufferBytes, 1, MALLOC_CAP_SPIRAM)));
    if (!m_pending || !m_text) {
        ESP_LOGE(TAG, "PSRAM buffer allocation failed");
        m_pending.reset();
        m_text.reset();
        return ESP_ERR_NO_MEM;
    }

    resolveComponentTasks();

    const BaseType_t result =
        xTaskCreatePinnedToCore(&MetricsReporter::taskEntry, kTask.name, kTask.stackSize, this,
                                kTask.priority, &m_task, kTask.core);
    if (result != pdPASS) {
        m_task = nullptr;
        ESP_LOGE(TAG, "Reporter task creation failed");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void MetricsReporter::resolveComponentTasks() {
    for (size_t i = 0; i < Diagnostics::kTaskCount; ++i) {
        if (kTaskInfo[i].resolveByName) {
            Diagnostics::Metrics::registerTask(static_cast<TaskId>(i),
                                               xTaskGetHandle(kTaskInfo[i].spec.name));
        }
    }
}

void MetricsReporter::taskEntry(void* arg) {
    static_cast<MetricsReporter*>(arg)->run();
}

void MetricsReporter::run() {
    Diagnostics::Metrics::registerTask(TaskId::Reporter, xTaskGetCurrentTaskHandle());

    const int64_t startUs = esp_timer_get_time();
    resetBlockAccumulators(startUs, Diagnostics::Metrics::peekCounter(Counter::BusCycles),
                           Diagnostics::Metrics::peekCounter(Counter::ImuSamples));
    m_lastEndUs = startUs;

    while (true) {
        uint32_t bits = 0;
        const uint32_t waitMs =
            m_signal != Status::ActivitySignal::None ? kSignalRefreshMs : kSampleIntervalMs;
        const BaseType_t notified = xTaskNotifyWait(0, ULONG_MAX, &bits, pdMS_TO_TICKS(waitMs));
        const int64_t nowUs = esp_timer_get_time();

        if (notified == pdTRUE) {
            handleNotifications(bits, nowUs);
        }
        if (m_sessionActive) {
            sampleHeap();
        }
        updateRates(nowUs);
        updateAudioIdle(nowUs);
        updateSignal(nowUs);

        if (flushDue(nowUs)) {
            flush(nowUs);
        }
    }
}

void MetricsReporter::handleNotifications(uint32_t bits, int64_t nowUs) {
    const bool begin = (bits & Diagnostics::Metrics::kSessionBeginBit) != 0;
    const bool end = (bits & Diagnostics::Metrics::kSessionEndBit) != 0;

    if (m_sessionActive) {
        if (end) endSession(nowUs);
        if (begin) beginSession(nowUs);
    } else {
        if (begin) beginSession(nowUs);
        if (end) endSession(nowUs);
    }
}

void MetricsReporter::beginSession(int64_t nowUs) {
    m_sessionActive = true;
    m_sessionStartUs = nowUs;

    if (!m_heapStartCaptured) {
        m_heapStartCaptured = true;
        m_heap.internalStart = internalFree();
        m_heap.internalMin = m_heap.internalStart;
        m_heap.internalLargestMin = internalLargestBlock();
        m_heap.psramStart = psramFree();
        m_heap.psramMin = m_heap.psramStart;
    }
    sampleHeap();
}

void MetricsReporter::endSession(int64_t nowUs) {
    if (!m_sessionActive) return;

    sampleHeap();
    m_heap.internalEnd = internalFree();
    m_heap.psramEnd = psramFree();
    m_heap.internalMinSinceBoot =
        static_cast<uint32_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));

    m_onTimeUs += nowUs - m_sessionStartUs;
    m_sessionActive = false;
    m_lastEndUs = nowUs;

    if (m_pendingCount < kMaxPendingBlocks) {
        captureBlock(m_pending[(m_pendingHead + m_pendingCount) % kMaxPendingBlocks], nowUs);
        ++m_pendingCount;
        resetBlockAccumulators(nowUs, 0, 0);
    } else {
        ++m_sessionsLost;
    }
}

void MetricsReporter::sampleHeap() {
    m_heap.internalMin = std::min(m_heap.internalMin, internalFree());
    m_heap.internalLargestMin = std::min(m_heap.internalLargestMin, internalLargestBlock());
    m_heap.psramMin = std::min(m_heap.psramMin, psramFree());
}

void MetricsReporter::updateRates(int64_t nowUs) {
    const int64_t elapsedUs = nowUs - m_rateWindowStartUs;
    if (elapsedUs < static_cast<int64_t>(kRateWindowMs) * kUsPerMs) return;

    const auto updateWindow = [elapsedUs](RateWindow& window, uint32_t count) {
        const uint32_t delta = count - window.lastCount;
        const auto hz = static_cast<uint32_t>(static_cast<uint64_t>(delta) * 1'000'000ULL /
                                              static_cast<uint64_t>(elapsedUs));
        window.minHz = window.hasSample ? std::min(window.minHz, hz) : hz;
        window.maxHz = window.hasSample ? std::max(window.maxHz, hz) : hz;
        window.hasSample = true;
        window.lastCount = count;
    };
    updateWindow(m_busRate, Diagnostics::Metrics::peekCounter(Counter::BusCycles));
    updateWindow(m_imuRate, Diagnostics::Metrics::peekCounter(Counter::ImuSamples));
    m_rateWindowStartUs = nowUs;
}

void MetricsReporter::updateAudioIdle(int64_t nowUs) {
    if (m_audio.getOutputLevel() <= kAudioIdleLevel) {
        if (!m_audioIdle) {
            m_audioIdle = true;
            m_audioIdleSinceUs = nowUs;
        }
    } else {
        m_audioIdle = false;
    }
}

void MetricsReporter::resetBlockAccumulators(int64_t nowUs, uint32_t busCyclesBaseline,
                                             uint32_t imuSamplesBaseline) {
    m_blockStartUs = nowUs;
    m_onTimeUs = 0;
    m_heapStartCaptured = false;
    m_heap = {};
    for (uint8_t core = 0; core < Diagnostics::kCoreCount; ++core) {
        m_blockStartCoreAllocations[core] = Diagnostics::Metrics::coreAllocations(core);
    }
    m_rateWindowStartUs = nowUs;
    m_busRate = {.lastCount = busCyclesBaseline, .minHz = 0, .maxHz = 0, .hasSample = false};
    m_imuRate = {.lastCount = imuSamplesBaseline, .minHz = 0, .maxHz = 0, .hasSample = false};
    m_blockStartBusCycles = busCyclesBaseline;
    m_blockStartImuSamples = imuSamplesBaseline;
}

void MetricsReporter::captureBlock(BlockSnapshot& block, int64_t nowUs) {
    const int64_t windowUs = nowUs - m_blockStartUs;

    block.block = m_nextBlock++;
    block.windowMs = toMs(windowUs);
    block.onTimeMs = toMs(m_onTimeUs);
    block.lostBefore = m_sessionsLost;
    m_sessionsLost = 0;

    Diagnostics::Metrics::snapshotAndReset(block.live);

    const auto averageTenths = [windowUs](uint32_t count) {
        if (windowUs <= 0) return uint32_t{0};
        return static_cast<uint32_t>(static_cast<uint64_t>(count) * kTenthsPerSecondUs /
                                     static_cast<uint64_t>(windowUs));
    };
    block.busRate = {
        m_busRate.minHz, m_busRate.maxHz,
        averageTenths(block.live.counters[index(Counter::BusCycles)] - m_blockStartBusCycles)};
    block.imuRate = {
        m_imuRate.minHz, m_imuRate.maxHz,
        averageTenths(block.live.counters[index(Counter::ImuSamples)] - m_blockStartImuSamples)};

    block.heap = m_heap;

    for (size_t i = 0; i < Diagnostics::kTaskCount; ++i) {
        const auto id = static_cast<TaskId>(i);
        if (id == TaskId::Main) {
            const Diagnostics::Metrics::BootRecord boot = Diagnostics::Metrics::bootRecord();
            block.tasks[i] = {true, boot.mainStackFreeMin, boot.mainPriority, boot.mainCore};
        } else if (TaskHandle_t handle = Diagnostics::Metrics::taskHandle(id); handle != nullptr) {
            block.tasks[i] = {true, static_cast<uint32_t>(uxTaskGetStackHighWaterMark(handle)),
                              uxTaskPriorityGet(handle), xTaskGetCoreID(handle)};
        } else {
            block.tasks[i] = {false, 0, 0, 0};
        }
    }

    for (uint8_t core = 0; core < Diagnostics::kCoreCount; ++core) {
        block.coreAllocations[core] =
            Diagnostics::Metrics::coreAllocations(core) - m_blockStartCoreAllocations[core];
    }
}

bool MetricsReporter::sdQuiet() const {
    return m_audioCache.preloadStatus() != PsramAudioCache::PreloadStatus::Pending &&
           !m_profiles.switchPending() && !m_profiles.savePending();
}

bool MetricsReporter::flushDue(int64_t nowUs) const {
    if (m_sessionActive || Diagnostics::Metrics::sessionActive()) return false;
    if (m_pendingCount == 0 && m_bootBlockWritten) return false;
    if (nowUs < m_nextFlushAttemptUs) return false;
    if (!sdQuiet()) return false;
    if (nowUs - m_lastEndUs < static_cast<int64_t>(kFlushSettleMs) * kUsPerMs) return false;
    return m_audioIdle &&
           nowUs - m_audioIdleSinceUs >= static_cast<int64_t>(kAudioIdleHoldMs) * kUsPerMs;
}

void MetricsReporter::flush(int64_t nowUs) {
    startSignal(Status::ActivitySignal::Writing, nowUs);
    const FlushOutcome outcome = writePending();
    const int64_t doneUs = esp_timer_get_time();
    m_nextFlushAttemptUs = doneUs + static_cast<int64_t>(kFlushSettleMs) * kUsPerMs;

    switch (outcome) {
    case FlushOutcome::Written:
        startSignal(Status::ActivitySignal::Written, doneUs);
        break;
    case FlushOutcome::Failed:
        startSignal(Status::ActivitySignal::WriteFailed, doneUs);
        break;
    case FlushOutcome::Postponed:
        startSignal(Status::ActivitySignal::None, doneUs);
        break;
    }
}

MetricsReporter::FlushOutcome MetricsReporter::writePending() {
    if (!m_filePathResolved) {
        m_filePathResolved = resolveFilePath();
        if (!m_filePathResolved) return FlushOutcome::Failed;
    }

    bool attempted = false;
    bool allWritten = true;
    if (!m_bootBlockWritten) {
        if (Diagnostics::Metrics::sessionActive() || !sdQuiet()) return FlushOutcome::Postponed;
        const WriteOutcome outcome = writeBootBlock();
        if (outcome == WriteOutcome::Deferred) return FlushOutcome::Failed;
        m_bootBlockWritten = true;
        attempted = true;
        allWritten = outcome == WriteOutcome::Written;
    }

    while (m_pendingCount > 0 && !Diagnostics::Metrics::sessionActive() && sdQuiet()) {
        const WriteOutcome outcome = writeSessionBlock(m_pending[m_pendingHead]);
        if (outcome == WriteOutcome::Deferred) return FlushOutcome::Failed;
        m_pendingHead = (m_pendingHead + 1) % kMaxPendingBlocks;
        --m_pendingCount;
        attempted = true;
        allWritten = allWritten && outcome == WriteOutcome::Written;
    }

    if (!attempted) return FlushOutcome::Postponed;
    return allWritten ? FlushOutcome::Written : FlushOutcome::Failed;
}

void MetricsReporter::startSignal(Status::ActivitySignal signal, int64_t nowUs) {
    const uint32_t durationMs = signal == Status::ActivitySignal::WriteFailed ? kWriteFailedSignalMs
                                : signal == Status::ActivitySignal::Written   ? kWrittenSignalMs
                                                                              : 0;
    m_signal = signal;
    m_signalEndUs = nowUs + static_cast<int64_t>(durationMs) * kUsPerMs;
    m_status.showActivity(signal);
}

void MetricsReporter::updateSignal(int64_t nowUs) {
    if (m_signal == Status::ActivitySignal::None) return;
    if (nowUs >= m_signalEndUs) {
        m_signal = Status::ActivitySignal::None;
    }
    m_status.showActivity(m_signal);
}

void MetricsReporter::warnSdUnavailable(const char* path) {
    if (!m_sdWarningLogged) {
        m_sdWarningLogged = true;
        ESP_LOGW(TAG, "Cannot access %s (errno %d); retrying silently", path, errno);
    }
}

bool MetricsReporter::resolveFilePath() {
    std::array<char, kDirectoryCapacity> directory{};
    std::snprintf(directory.data(), directory.size(), "%s/metrics",
                  Hardware::HardwareConfig::kSdMountPoint);

    if (mkdir(directory.data(), 0775) != 0 && errno != EEXIST) {
        warnSdUnavailable(directory.data());
        return false;
    }

    UniqueDir dir = openDir(directory.data());
    if (!dir) {
        warnSdUnavailable(directory.data());
        return false;
    }
    m_sdWarningLogged = false;

    uint32_t highest = 0;
    while (const dirent* entry = readdir(dir.get())) {
        uint32_t fileIndex = 0;
        if (parseFileIndex(entry->d_name, fileIndex)) {
            highest = std::max(highest, fileIndex);
        }
    }

    uint32_t next = highest + 1;
    if (next > kMaxFileIndex) {
        next = kMaxFileIndex;
        ESP_LOGW(TAG, "Session file index exhausted: appending to session_%03" PRIu32 ".csv", next);
    }

    std::snprintf(m_filePath.data(), m_filePath.size(), "%s/session_%03" PRIu32 ".csv",
                  directory.data(), next);
    return true;
}

MetricsReporter::WriteOutcome MetricsReporter::writeBootBlock() {
    UniqueFile file = openFile(m_filePath.data(), "a");
    if (!file) {
        warnSdUnavailable(m_filePath.data());
        return WriteOutcome::Deferred;
    }
    m_sdWarningLogged = false;

    const esp_app_desc_t* app = esp_app_get_description();
    std::array<char, 17> elfSha{};
    esp_app_get_elf_sha256(elfSha.data(), elfSha.size());
    std::array<char, sizeof(app->date) + sizeof(app->time) + 1> buildTime{};
    std::snprintf(buildTime.data(), buildTime.size(), "%s %s", app->date, app->time);
    const Diagnostics::Metrics::BootRecord boot = Diagnostics::Metrics::bootRecord();

    CsvWriter csv(file.get(), m_text.get(), kTextBufferBytes, 0);
    csv.line("block,section,name,stat,value,unit");
    csv.number("meta", "schema", "", kSchemaVersion, "");
    csv.text("meta", "fw_version", "", app->version, "");
    csv.text("meta", "build_time", "", buildTime.data(), "");
    csv.text("meta", "idf_version", "", app->idf_ver, "");
    csv.text("meta", "elf_sha256", "", elfSha.data(), "");
    csv.number("meta", "cpu_freq", "", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, "MHz");
    csv.number("meta", "tick_rate", "", CONFIG_FREERTOS_HZ, "Hz");
    csv.number("meta", "log_level", "", CONFIG_LOG_DEFAULT_LEVEL, "");
    csv.text("meta", "optimization", "", optimizationLevel(), "");
    csv.number("boot", "duration", "", boot.durationMs, "ms");
    const esp_reset_reason_t resetReason = esp_reset_reason();
    if (const char* name = resetReasonName(resetReason); name != nullptr) {
        csv.text("boot", "reset_reason", "", name, "");
    } else {
        csv.number("boot", "reset_reason", "", static_cast<uint32_t>(resetReason), "");
    }
    csv.number("boot", "heap_internal_free", "", boot.heapInternalFree, "B");
    csv.number("boot", "heap_psram_free", "", boot.heapPsramFree, "B");
    csv.number("task", kTaskInfo[static_cast<size_t>(TaskId::Main)].name, "stack_free_min",
               boot.mainStackFreeMin, "B");

    const bool written = csv.finish();
    if (!closeFile(std::move(file)) || !written) {
        ESP_LOGW(TAG, "Boot block write to %s failed; not retried", m_filePath.data());
        return WriteOutcome::Dropped;
    }
    ESP_LOGI(TAG, "Boot block written to %s", m_filePath.data());
    return WriteOutcome::Written;
}

MetricsReporter::WriteOutcome MetricsReporter::writeSessionBlock(const BlockSnapshot& block) {
    UniqueFile file = openFile(m_filePath.data(), "a");
    if (!file) {
        warnSdUnavailable(m_filePath.data());
        return WriteOutcome::Deferred;
    }
    m_sdWarningLogged = false;

    const Diagnostics::Metrics::Snapshot& live = block.live;
    CsvWriter csv(file.get(), m_text.get(), kTextBufferBytes, block.block);

    csv.number("session", "window", "", block.windowMs, "ms");
    csv.number("session", "on_time", "", block.onTimeMs, "ms");
    csv.number("session", "lost_before", "", block.lostBefore, "");

    const auto rateRows = [&csv](const char* name, const RateStats& rate) {
        csv.number("rate", name, "min", rate.minHz, "Hz");
        csv.tenths("rate", name, "avg", rate.avgTenthsHz, "Hz");
        csv.number("rate", name, "max", rate.maxHz, "Hz");
    };
    rateRows("bus", block.busRate);
    rateRows("imu", block.imuRate);

    for (size_t i = 0; i < Diagnostics::kMetricCount; ++i) {
        const MetricInfo& info = kMetricInfo[i];
        const Diagnostics::Metrics::DurationStats& stats = live.durations[i];
        const uint32_t average = stats.count > 0 ? stats.sumUs / stats.count : 0;

        if (info.kind != MetricKind::Interval) {
            csv.number("time", info.name, "count", stats.count, "");
        }
        csv.number("time", info.name, "avg", average, "us");
        csv.number("time", info.name, "max", stats.maxUs, "us");
        if (info.kind == MetricKind::Scope) {
            csv.number("time", info.name, "allocs", stats.allocations, "");
            csv.number("time", info.name, "scopes_with_alloc", stats.scopesWithAllocations, "");
        }

        if (i == index(Metric::BusCycle)) {
            for (size_t bucket = 0; bucket < Diagnostics::kBusCycleBucketCount; ++bucket) {
                csv.number("hist", info.name, kBusCycleBucketNames[bucket],
                           live.busCycleHistogram[bucket], "");
            }
        }
    }

    size_t worstRun = kFirstRunMetric;
    uint32_t runAllocations = 0;
    for (size_t i = kFirstRunMetric; i <= kLastRunMetric; ++i) {
        if (live.durations[i].maxUs > live.durations[worstRun].maxUs) worstRun = i;
        runAllocations += live.durations[i].allocations;
    }
    const uint32_t worstRunUs = live.durations[worstRun].maxUs;
    csv.text("worst", "run", "name", worstRunUs > 0 ? kMetricInfo[worstRun].name : "none", "");
    csv.number("worst", "run", "max", worstRunUs, "us");

    const uint32_t busCycleAllocations = live.durations[index(Metric::BusCycle)].allocations;
    const uint32_t busLoopAllocations =
        busCycleAllocations > runAllocations ? busCycleAllocations - runAllocations : 0;
    csv.number("derived", "bus_loop_allocs", "", busLoopAllocations, "");

    for (size_t i = 0; i < Diagnostics::kCounterCount; ++i) {
        csv.number("count", kCounterNames[i], "", live.counters[i], "");
    }

    const HeapStats& heap = block.heap;
    csv.number("heap", "internal_free", "start", heap.internalStart, "B");
    csv.number("heap", "internal_free", "min", heap.internalMin, "B");
    csv.number("heap", "internal_free", "end", heap.internalEnd, "B");
    csv.number("heap", "internal_largest_block", "min", heap.internalLargestMin, "B");
    csv.number("heap", "internal_min_since_boot", "", heap.internalMinSinceBoot, "B");
    csv.number("heap", "psram_free", "start", heap.psramStart, "B");
    csv.number("heap", "psram_free", "min", heap.psramMin, "B");
    csv.number("heap", "psram_free", "end", heap.psramEnd, "B");

    bool stackMarginOk = true;
    for (size_t i = 0; i < Diagnostics::kTaskCount; ++i) {
        const TaskInfo& info = kTaskInfo[i];
        const TaskReading& task = block.tasks[i];
        csv.number("task", info.name, "size", info.spec.stackSize, "B");
        if (task.known) {
            csv.number("task", info.name, "stack_free_min", task.stackFreeMin, "B");
            csv.number("task", info.name, "priority", static_cast<uint32_t>(task.priority), "");
            if (task.core == tskNO_AFFINITY) {
                csv.text("task", info.name, "core", "any", "");
            } else {
                csv.number("task", info.name, "core", static_cast<uint32_t>(task.core), "");
            }
            stackMarginOk = stackMarginOk && task.stackFreeMin >= kStackMarginBytes;
        } else {
            csv.text("task", info.name, "stack_free_min", "missing", "B");
            csv.text("task", info.name, "priority", "missing", "");
            csv.text("task", info.name, "core", "missing", "");
        }
    }

    csv.number("alloc", "core0", "", block.coreAllocations[0], "");
    csv.number("alloc", "core1", "", block.coreAllocations[1], "");

    const uint32_t flowAllocations = live.durations[index(Metric::RunSwing)].allocations +
                                     live.durations[index(Metric::RunPreloadWait)].allocations;
    const bool flowAllocationsOk =
        flowAllocations == 0 && live.durations[index(Metric::RunLight)].scopesWithAllocations <=
                                    live.counters[index(Counter::InertialBursts)];
    csv.text("check", "bus_cycle_max_lt_2ms", "",
             passFail(live.durations[index(Metric::BusCycle)].maxUs < kBusCycleBudgetUs), "");
    csv.text("check", "bus_loop_allocs_zero", "", passFail(busLoopAllocations == 0), "");
    csv.text("check", "flow_effects_allocs_zero", "", passFail(flowAllocationsOk), "");
    csv.text("check", "stack_margin_ge_1k", "", passFail(stackMarginOk), "");

    const bool written = csv.finish();
    if (!closeFile(std::move(file)) || !written) {
        ESP_LOGW(TAG, "Block %" PRIu32 " write to %s failed; dropped", block.block,
                 m_filePath.data());
        ++m_sessionsLost;
        return WriteOutcome::Dropped;
    }
    ESP_LOGI(TAG, "Block %" PRIu32 " written to %s", block.block, m_filePath.data());
    return WriteOutcome::Written;
}

} // namespace InertialSaber::System::Monitoring

#endif
