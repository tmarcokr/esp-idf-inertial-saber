// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "BusConfig.hpp"
#include "EffectSet.hpp"
#include "SaberDataPacket.hpp"
#include "PhysicsConfig.hpp"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace InertialSaber::Core {

/**
 * @brief Queued input event pushed by external InputAdapters.
 */
struct InputEvent {
    uint8_t inputId;
    InputDescriptor descriptor;
};

/**
 * @brief Asynchronous event dispatcher for the InertialSaber OS.
 *
 * Runs the active EffectSet: builds a SaberDataPacket each cycle from externally
 * injected motion and input data, and evaluates every effect of the set. Uses a
 * hybrid event-driven model: the task blocks on a FreeRTOS notification with a
 * short timeout fallback to ensure continuous evaluation for Flow Modulator effects.
 *
 * Thread safety:
 *   - The active set is touched only by the bus task.
 *   - installEffects(): only while the bus is stopped.
 *   - stageEffects() / takeRetiredEffects(): the only cross-task hand-off; one stager
 *     task at a time. A staged set becomes active at the top of a later cycle and the
 *     set it replaces is published for the stager to take.
 *   - Sets are never destroyed on the bus task.
 *   - updateMotion(): safe from any task (spinlock-guarded sample copy).
 *   - pushInputEvent(): safe from any task (FreeRTOS queue).
 */
class SaberActionBus {
public:
    /**
     * @brief Construct a stopped bus.
     * @param config Task and motion filter parameters, copied and immutable afterwards.
     */
    explicit SaberActionBus(const BusConfig& config);

    /**
     * @brief Stop the bus (see stop()), destroy any staged or retired set, then release the exit
     * semaphore.
     */
    ~SaberActionBus();

    SaberActionBus(const SaberActionBus&) = delete;
    SaberActionBus& operator=(const SaberActionBus&) = delete;

    /**
     * @brief Create the input queue and spawn the bus task.
     * @return ESP_OK on success, ESP_FAIL if already running or resource allocation fails.
     */
    [[nodiscard]] esp_err_t start();

    /**
     * @brief Signal the bus task to exit, join it without a timeout and delete the input queue.
     *
     * Idempotent: returns immediately when the bus is not running. Must not be called from the
     * bus task itself.
     */
    void stop();

    /**
     * @brief Installs the initial set and applies its physics. Bus stopped only; used at boot.
     * @param set The set to run; any previously installed set is destroyed in the caller's task.
     */
    void installEffects(std::unique_ptr<EffectSet> set);

    /**
     * @brief Hands @p set to the bus; it becomes active at the top of a later cycle.
     *
     * Any task except the bus.
     * @return false if @p set is null or a set is already staged; @p set is then destroyed in the
     *         caller's task.
     */
    [[nodiscard]] bool stageEffects(std::unique_ptr<EffectSet> set);

    /**
     * @brief Takes the set replaced by the last commit. Stager task only.
     * @return The retired set, or null if none is waiting.
     */
    [[nodiscard]] std::unique_ptr<EffectSet> takeRetiredEffects();

    /**
     * @brief Inject updated motion data from an external IMU adapter.
     *
     * The whole sample is published atomically. Safe to call from any task.
     * @param sample Latest motion sample.
     */
    void updateMotion(const MotionSample& sample);

    /**
     * @brief Push a button state change into the bus input queue.
     *
     * Called by InputAdapters when a state transition is detected.
     * Automatically wakes the bus task via notification. Never blocks: when the
     * queue is full the event is dropped and the bus task logs the drop count.
     *
     * @param inputId Button index (0...kMaxInputs-1).
     * @param descriptor Full state snapshot at the moment of transition.
     */
    void pushInputEvent(uint8_t inputId, const InputDescriptor& descriptor);

private:
    static constexpr uint32_t kBusTimeoutMs = 10;
    static constexpr uint8_t kInputQueueDepth = 8;

    const BusConfig m_config;

    StaticSemaphore_t m_exitSemaphoreControl{};
    SemaphoreHandle_t m_exitSemaphore = nullptr;

    std::atomic<TaskHandle_t> m_taskHandle{nullptr};
    std::atomic<QueueHandle_t> m_inputQueue{nullptr};
    std::atomic<bool> m_running{false};
    std::atomic<uint32_t> m_droppedInputEvents{0};

    std::unique_ptr<EffectSet> m_activeEffects;
    std::atomic<EffectSet*> m_stagedEffects{nullptr};
    std::atomic<EffectSet*> m_retiredEffects{nullptr};
    SaberDataPacket m_packet{};

    float m_overloadLevel = 0.0f;
    uint32_t m_lastBurstTimeMs = 0;
    int64_t m_lastLoopTimeUs = 0;

    float m_kineticEnergyDeadbandG = 0.25f;
    float m_rotationDeadbandDps = 15.0f;
    float m_overloadThresholdG = 1.0f;
    float m_overloadChargeRate = 2.0f;
    float m_overloadDrainRate = 0.5f;
    float m_burstCooldownMs = 1500.0f;

    // Warning: m_motionLock is a spinlock; keep its critical sections to a plain struct copy.
    portMUX_TYPE m_motionLock = portMUX_INITIALIZER_UNLOCKED;
    MotionSample m_stagedMotion{};

    static void busTaskEntry(void* arg);
    void busLoop();
    void commitStagedEffects();
    void applyPhysics(const PhysicsConfig& physics);
    void drainInputQueue();
    void applyStagedMotion();
    void loadStagedMotionToPacket();
    void filterStagedMotionWarmUp();
    void filterStagedMotionStabilization();
    void filterStagedMotionOrientation();
    void computeInertialOverload();
    bool isInertialOverloadInCooldown() const;
    void resetInertialOverloadState();
    void chargeOrDrainInertialOverload(float dtSec);
    void clampInertialOverloadLevel();
    void evaluateInertialBurst();
    float calculateDeltaTimeSec();
};

} // namespace InertialSaber::Core
