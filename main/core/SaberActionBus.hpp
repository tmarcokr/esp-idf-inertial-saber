#pragma once

#include "InertialEffect.hpp"
#include "PlatformConfig.hpp"
#include "SaberDataPacket.hpp"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <cstdint>
#include <memory>
#include <vector>

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
 * Maintains the active InertialEffect list, builds a SaberDataPacket each
 * cycle from externally injected motion and input data, and evaluates all
 * registered effects. Uses a hybrid event-driven model: the task blocks on
 * a FreeRTOS notification with a short timeout fallback to ensure continuous
 * evaluation for Flow Modulator effects.
 *
 * Thread safety:
 *   - registerEffect() / clearEffects(): call only when the bus is stopped
 *     or from the bus task context (profile loading).
 *   - updateMotion(): safe from any task (atomic field writes).
 *   - pushInputEvent(): safe from any task (FreeRTOS queue).
 */
class SaberActionBus {
public:
    SaberActionBus();
    ~SaberActionBus();

    SaberActionBus(const SaberActionBus&) = delete;
    SaberActionBus& operator=(const SaberActionBus&) = delete;

    /**
     * @brief Create the input queue and spawn the bus task.
     * @return ESP_OK on success, ESP_FAIL if already running or resource allocation fails.
     */
    [[nodiscard]] esp_err_t start();

    /**
     * @brief Signal the bus task to terminate and wait for cleanup.
     */
    void stop();

    /**
     * @brief Register an InertialEffect on the bus. Ownership is transferred.
     * @param effect The effect to register. Must not be null.
     */
    void registerEffect(std::unique_ptr<InertialEffect> effect);

    /**
     * @brief Remove and destroy all registered effects.
     */
    void clearEffects();

    /**
     * @brief Inject updated motion data from an external IMU adapter.
     *
     * Fields are copied individually (small POD). Safe to call from any task.
     */
    void updateMotion(float energy, const float rotation[3], float orientation);

    /**
     * @brief Push a button state change into the bus input queue.
     *
     * Called by InputAdapters when a state transition is detected.
     * Automatically wakes the bus task via notification.
     *
     * @param inputId Button index (0...kMaxInputs-1).
     * @param descriptor Full state snapshot at the moment of transition.
     */
    void pushInputEvent(uint8_t inputId, const InputDescriptor& descriptor);

    /**
     * @brief Get the bus task handle for external notification sources.
     */
    TaskHandle_t getTaskHandle() const;

private:
    TaskHandle_t m_taskHandle = nullptr;
    QueueHandle_t m_inputQueue = nullptr;
    volatile bool m_running = false;

    std::vector<std::unique_ptr<InertialEffect>> m_effects;
    SaberDataPacket m_packet{};

    // Atomic-safe motion staging area written by updateMotion()
    volatile float m_stagedEnergy = 0.0f;
    volatile float m_stagedRotation[3] = {0.0f, 0.0f, 0.0f};
    volatile float m_stagedOrientation = 0.0f;

    static void busTaskEntry(void* arg);
    void busLoop();
    void drainInputQueue();
    void applyStagedMotion();
};

} // namespace InertialSaber::Core
