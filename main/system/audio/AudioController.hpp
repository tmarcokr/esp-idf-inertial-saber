#pragma once

#include "system/audio/AudioPath.hpp"
#include "system/hardware/HardwareConfig.hpp"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Espressif::Wrappers::Audio {
class AudioEngine;
}

namespace InertialSaber::System {

class AudioController;

/**
 * @brief Move-only handle to one persistent playback voice of an AudioController.
 *
 * A voice binds at most one engine channel at a time. play() and stop() are queued to the
 * controller task and never block; setVolume() is applied directly and lock-free. Destroying
 * the handle stops the voice and returns its slot to the controller.
 */
class AudioVoice {
public:
    /** @brief Creates an invalid voice that ignores every call. */
    AudioVoice() = default;
    ~AudioVoice();

    AudioVoice(AudioVoice&& other) noexcept;
    AudioVoice& operator=(AudioVoice&& other) noexcept;
    AudioVoice(const AudioVoice&) = delete;
    AudioVoice& operator=(const AudioVoice&) = delete;

    /**
     * @brief Queues playback of @p path, replacing whatever the voice was playing.
     * @return false if the voice is invalid, the path overflowed or the command queue is full.
     */
    bool play(const AudioPath& path, bool loop, uint16_t volume);

    /** @brief Queues a fade-out stop of the voice's current channel. */
    void stop();

    /** @brief Sets the target volume (0-16384) of the current and any later channel; lock-free. */
    void setVolume(uint16_t volume);

    [[nodiscard]] bool valid() const { return m_controller != nullptr; }

private:
    friend class AudioController;

    AudioVoice(AudioController& controller, uint8_t slot, uint32_t generation);

    void release();

    AudioController* m_controller = nullptr;
    uint8_t m_slot = 0;
    uint32_t m_generation = 0;
};

/**
 * @brief Active object that owns every blocking AudioEngine call (play/stop).
 *
 * Real-time callers enqueue commands without blocking; a dedicated task executes them in FIFO
 * order. When the queue is full the command is dropped and counted. Voice volumes bypass the
 * queue through the engine's lock-free setChannelVolume().
 */
class AudioController {
public:
    /** @brief Voice slots; two profiles hold voices at once during a profile swap. */
    static constexpr size_t kMaxVoices = 12;
    static constexpr size_t kQueueDepth = 16;

    /**
     * @param engine Started audio engine; must outlive the controller.
     * @param task Parameters of the controller task.
     */
    AudioController(Espressif::Wrappers::Audio::AudioEngine& engine,
                    const Hardware::TaskSpec& task);

    /** @brief Queues a shutdown behind pending commands and blocks until the task has exited. */
    ~AudioController();

    AudioController(const AudioController&) = delete;
    AudioController& operator=(const AudioController&) = delete;

    /**
     * @brief Spawns the controller task.
     * @return ESP_OK; ESP_ERR_INVALID_STATE if already started; ESP_ERR_NO_MEM if the task
     *         cannot be created.
     */
    [[nodiscard]] esp_err_t start();

    /**
     * @brief Queues a one-shot playback of @p path; never blocks.
     * @return false if the path overflowed or is empty, or the command queue is full.
     */
    bool playOneShot(const AudioPath& path, uint16_t volume);

    /**
     * @brief Reserves a persistent voice; call outside the real-time path (effect construction).
     * @return A valid voice, or an invalid one if all kMaxVoices slots are in use.
     */
    [[nodiscard]] AudioVoice acquireVoice();

private:
    friend class AudioVoice;

    enum class CommandType : uint8_t { PlayOneShot, PlayVoice, StopVoice, ReleaseVoice, Shutdown };

    struct Command {
        CommandType type;
        uint8_t voice;
        bool loop;
        uint16_t volume;
        uint32_t generation;
        uint32_t enqueuedUs;
        AudioPath path;
    };

    static constexpr int32_t kNoChannel = -1;

    struct VoiceSlot {
        std::atomic<bool> inUse{false};
        std::atomic<bool> releasePending{false};
        std::atomic<uint32_t> generation{0};
        std::atomic<int32_t> channel{kNoChannel};
        std::atomic<uint32_t> volume{0};
    };

    bool enqueue(const Command& command);
    bool enqueueVoicePlay(uint8_t voice, uint32_t generation, const AudioPath& path, bool loop,
                          uint16_t volume);
    void enqueueVoiceStop(uint8_t voice, uint32_t generation);
    void setVoiceVolume(uint8_t voice, uint16_t volume);
    void releaseVoice(uint8_t voice);

    static void taskEntry(void* arg);
    void run();
    void execute(const Command& command);
    int32_t playTimed(const Command& command);
    void playVoice(const Command& command);
    void stopVoiceChannel(VoiceSlot& slot);
    void sweepReleasedVoices();
    void reportDroppedCommands();
    [[nodiscard]] bool ownsSlot(const VoiceSlot& slot, uint32_t generation) const;

    Espressif::Wrappers::Audio::AudioEngine& m_engine;
    const Hardware::TaskSpec m_taskSpec;

    std::array<VoiceSlot, kMaxVoices> m_voices{};
    std::atomic<uint32_t> m_droppedCommands{0};

    StaticQueue_t m_queueControl{};
    std::array<uint8_t, kQueueDepth * sizeof(Command)> m_queueStorage{};
    QueueHandle_t m_queue = nullptr;

    StaticSemaphore_t m_exitSemaphoreControl{};
    SemaphoreHandle_t m_exitSemaphore = nullptr;

    TaskHandle_t m_task = nullptr;
};

} // namespace InertialSaber::System
