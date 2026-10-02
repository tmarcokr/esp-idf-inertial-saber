#include "system/audio/AudioController.hpp"
#include "diagnostics/Metrics.hpp"
#include "system/PsramAudioCache.hpp"

#include "AudioEngine.hpp"

#include "esp_log.h"
#include "esp_timer.h"

#include <cinttypes>
#include <string_view>
#include <utility>

namespace InertialSaber::System {

namespace {

constexpr const char* TAG = "AudioController";

using Espressif::Wrappers::Audio::ChannelId;
using Espressif::Wrappers::Audio::INVALID_CHANNEL;

#if CONFIG_STDATOMIC_S32C1I_SPIRAM_WORKAROUND
// Warning: with PSRAM, IDF atomics are lock-free only in internal RAM; keep this object there.
#else
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<uint32_t>::is_always_lock_free);
static_assert(std::atomic<int32_t>::is_always_lock_free);
#endif

uint32_t nowUs() {
    return static_cast<uint32_t>(esp_timer_get_time());
}

#if CONFIG_SABER_METRICS
Diagnostics::Metric playSourceMetric(std::string_view path) {
    constexpr std::string_view kMount = PsramAudioCache::kMountPoint;
    const bool fromMemory =
        path.starts_with(kMount) && path.size() > kMount.size() && path[kMount.size()] == '/';
    return fromMemory ? Diagnostics::Metric::AudioPlayCallMem
                      : Diagnostics::Metric::AudioPlayCallSd;
}
#endif

} // namespace

AudioVoice::AudioVoice(AudioController& controller, uint8_t slot, uint32_t generation)
    : m_controller(&controller)
    , m_slot(slot)
    , m_generation(generation) {}

AudioVoice::~AudioVoice() {
    release();
}

AudioVoice::AudioVoice(AudioVoice&& other) noexcept
    : m_controller(std::exchange(other.m_controller, nullptr))
    , m_slot(other.m_slot)
    , m_generation(other.m_generation) {}

AudioVoice& AudioVoice::operator=(AudioVoice&& other) noexcept {
    if (this != &other) {
        release();
        m_controller = std::exchange(other.m_controller, nullptr);
        m_slot = other.m_slot;
        m_generation = other.m_generation;
    }
    return *this;
}

bool AudioVoice::play(const AudioPath& path, bool loop, uint16_t volume) {
    return valid() && m_controller->enqueueVoicePlay(m_slot, m_generation, path, loop, volume);
}

void AudioVoice::stop() {
    if (valid()) m_controller->enqueueVoiceStop(m_slot, m_generation);
}

void AudioVoice::setVolume(uint16_t volume) {
    if (valid()) m_controller->setVoiceVolume(m_slot, volume);
}

void AudioVoice::release() {
    if (AudioController* controller = std::exchange(m_controller, nullptr); controller != nullptr) {
        controller->releaseVoice(m_slot);
    }
}

AudioController::AudioController(Espressif::Wrappers::Audio::AudioEngine& engine,
                                 const Hardware::TaskSpec& task)
    : m_engine(engine)
    , m_taskSpec(task)
    , m_queue(
          xQueueCreateStatic(kQueueDepth, sizeof(Command), m_queueStorage.data(), &m_queueControl))
    , m_exitSemaphore(xSemaphoreCreateBinaryStatic(&m_exitSemaphoreControl)) {}

AudioController::~AudioController() {
    if (m_task != nullptr) {
        configASSERT(xTaskGetCurrentTaskHandle() != m_task);
        Command shutdown{};
        shutdown.type = CommandType::Shutdown;
        xQueueSend(m_queue, &shutdown, portMAX_DELAY);
        xSemaphoreTake(m_exitSemaphore, portMAX_DELAY);
        m_task = nullptr;
    }
    vSemaphoreDelete(m_exitSemaphore);
    vQueueDelete(m_queue);
}

esp_err_t AudioController::start() {
    if (m_task != nullptr) return ESP_ERR_INVALID_STATE;

    if (xTaskCreatePinnedToCore(&AudioController::taskEntry, m_taskSpec.name, m_taskSpec.stackSize,
                                this, m_taskSpec.priority, &m_task, m_taskSpec.core) != pdPASS) {
        m_task = nullptr;
        ESP_LOGE(TAG, "%s task creation failed", m_taskSpec.name);
        return ESP_ERR_NO_MEM;
    }
    SABER_METRIC_REGISTER_TASK(Diagnostics::TaskId::AudioControl, m_task);
    return ESP_OK;
}

bool AudioController::playOneShot(const AudioPath& path, uint16_t volume) {
    if (!path.ok() || path.empty()) {
        SABER_METRIC_COUNT(Diagnostics::Counter::AudioPlayFailed);
        return false;
    }
    Command command{};
    command.type = CommandType::PlayOneShot;
    command.volume = volume;
    command.path = path;
    return enqueue(command);
}

AudioVoice AudioController::acquireVoice() {
    for (size_t i = 0; i < m_voices.size(); ++i) {
        VoiceSlot& slot = m_voices[i];
        bool expected = false;
        if (slot.inUse.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
            slot.volume.store(0, std::memory_order_seq_cst);
            return {*this, static_cast<uint8_t>(i),
                    slot.generation.load(std::memory_order_relaxed)};
        }
    }
    ESP_LOGE(TAG, "No free audio voice (%u in use)", static_cast<unsigned>(kMaxVoices));
    return {};
}

bool AudioController::enqueue(const Command& command) {
    Command stamped = command;
    stamped.enqueuedUs = nowUs();
    if (xQueueSend(m_queue, &stamped, 0) == pdTRUE) return true;

    SABER_METRIC_COUNT(Diagnostics::Counter::AudioCommandsDropped);
    m_droppedCommands.fetch_add(1, std::memory_order_relaxed);
    return false;
}

bool AudioController::enqueueVoicePlay(uint8_t voice, uint32_t generation, const AudioPath& path,
                                       bool loop, uint16_t volume) {
    if (!path.ok() || path.empty()) {
        SABER_METRIC_COUNT(Diagnostics::Counter::AudioPlayFailed);
        return false;
    }
    m_voices[voice].volume.store(volume, std::memory_order_seq_cst);

    Command command{};
    command.type = CommandType::PlayVoice;
    command.voice = voice;
    command.loop = loop;
    command.volume = volume;
    command.generation = generation;
    command.path = path;
    return enqueue(command);
}

void AudioController::enqueueVoiceStop(uint8_t voice, uint32_t generation) {
    Command command{};
    command.type = CommandType::StopVoice;
    command.voice = voice;
    command.generation = generation;
    enqueue(command);
}

void AudioController::setVoiceVolume(uint8_t voice, uint16_t volume) {
    VoiceSlot& slot = m_voices[voice];
    // Warning: seq_cst Dekker pair with playVoice(); weaker orders can lose the latest volume.
    slot.volume.store(volume, std::memory_order_seq_cst);
    if (const int32_t channel = slot.channel.load(std::memory_order_seq_cst);
        channel != kNoChannel) {
        m_engine.setChannelVolume(static_cast<ChannelId>(channel), volume);
    }
}

void AudioController::releaseVoice(uint8_t voice) {
    m_voices[voice].releasePending.store(true, std::memory_order_seq_cst);

    Command command{};
    command.type = CommandType::ReleaseVoice;
    command.voice = voice;
    enqueue(command);
}

void AudioController::taskEntry(void* arg) {
    static_cast<AudioController*>(arg)->run();
}

void AudioController::run() {
    Command command{};
    while (xQueueReceive(m_queue, &command, portMAX_DELAY) == pdTRUE &&
           command.type != CommandType::Shutdown) {
        execute(command);
        sweepReleasedVoices();
        reportDroppedCommands();
    }
    xSemaphoreGive(m_exitSemaphore);
    vTaskDelete(nullptr);
}

void AudioController::execute(const Command& command) {
    if (command.type == CommandType::PlayOneShot) {
        playTimed(command);
        return;
    }
    if (command.voice >= m_voices.size()) return;

    switch (command.type) {
    case CommandType::PlayVoice:
        playVoice(command);
        break;
    case CommandType::StopVoice:
        if (VoiceSlot& slot = m_voices[command.voice]; ownsSlot(slot, command.generation)) {
            stopVoiceChannel(slot);
        }
        break;
    case CommandType::PlayOneShot:
    case CommandType::ReleaseVoice:
    case CommandType::Shutdown:
        break;
    }
}

int32_t AudioController::playTimed(const Command& command) {
    ChannelId channel = INVALID_CHANNEL;
    {
        SABER_METRIC_SCOPE(Diagnostics::Metric::AudioPlayCall);
#if CONFIG_SABER_METRICS
        const uint32_t playStartUs = nowUs();
#endif
        channel = m_engine.play(command.path.view(), command.loop, command.volume);
        SABER_METRIC_DURATION(playSourceMetric(command.path.view()), nowUs() - playStartUs);
    }
    SABER_METRIC_DURATION(Diagnostics::Metric::AudioLatency, nowUs() - command.enqueuedUs);
    if (channel == INVALID_CHANNEL) {
        SABER_METRIC_COUNT(Diagnostics::Counter::AudioPlayFailed);
    }
    return channel;
}

void AudioController::playVoice(const Command& command) {
    static_assert(kNoChannel == INVALID_CHANNEL);
    VoiceSlot& slot = m_voices[command.voice];
    if (!ownsSlot(slot, command.generation)) return;

    stopVoiceChannel(slot);
    const int32_t channel = playTimed(command);
    if (channel == kNoChannel) return;

    // Warning: seq_cst Dekker pair with setVoiceVolume(); re-check so a stale volume never wins.
    slot.channel.store(channel, std::memory_order_seq_cst);
    uint32_t applied = slot.volume.load(std::memory_order_seq_cst);
    while (true) {
        m_engine.setChannelVolume(static_cast<ChannelId>(channel), static_cast<uint16_t>(applied));
        const uint32_t latest = slot.volume.load(std::memory_order_seq_cst);
        if (latest == applied) break;
        applied = latest;
    }
}

void AudioController::stopVoiceChannel(VoiceSlot& slot) {
    if (const int32_t channel = slot.channel.exchange(kNoChannel, std::memory_order_seq_cst);
        channel != kNoChannel) {
        m_engine.stop(static_cast<ChannelId>(channel));
    }
}

void AudioController::sweepReleasedVoices() {
    for (VoiceSlot& slot : m_voices) {
        if (!slot.releasePending.load(std::memory_order_seq_cst)) continue;
        stopVoiceChannel(slot);
        slot.generation.fetch_add(1, std::memory_order_relaxed);
        slot.releasePending.store(false, std::memory_order_relaxed);
        slot.inUse.store(false, std::memory_order_release);
    }
}

void AudioController::reportDroppedCommands() {
    if (const uint32_t dropped = m_droppedCommands.exchange(0, std::memory_order_relaxed);
        dropped > 0) {
        ESP_LOGW(TAG, "%" PRIu32 " audio command(s) dropped: queue full", dropped);
    }
}

bool AudioController::ownsSlot(const VoiceSlot& slot, uint32_t generation) const {
    return slot.generation.load(std::memory_order_relaxed) == generation &&
           !slot.releasePending.load(std::memory_order_seq_cst);
}

static_assert(AudioController::kMaxVoices <= UINT8_MAX);

} // namespace InertialSaber::System
