#pragma once

#include "core/SaberActionBus.hpp"
#include "core/SaberDataPacket.hpp"
#include "GpioButton.hpp"
#include "esp_timer.h"

#include <cstdint>
#include <mutex>

namespace InertialSaber::System::Adapters {

/**
 * @brief Translates raw GpioButton events into semantic InputDescriptor gestures.
 */
class InputAdapter {
public:
    /**
     * @brief Construct a new InputAdapter.
     */
    InputAdapter(Core::SaberActionBus& bus, Espressif::Wrappers::GpioButton& button);

    /**
     * @brief Destructor.
     */
    ~InputAdapter();

    InputAdapter(const InputAdapter&)            = delete;
    InputAdapter& operator=(const InputAdapter&) = delete;

    /**
     * @brief Starts the input adapter, registering callbacks and creating timers.
     */
    [[nodiscard]] esp_err_t start();

private:
    void onPressDown();
    void onPressUp();
    void onFirstHoldTick();
    void resolveClickGesture();
    void resolveHoldTick();
    uint8_t emitHoldTickLocked();

    static void clickTimerCallback(void* arg);
    static void holdTimerCallback(void* arg);

    Core::SaberActionBus&            m_bus;
    Espressif::Wrappers::GpioButton& m_mainButton;

    // Warning: m_stateMutex serialises the GpioButton poll task and the esp_timer task; hold it
    // only around state updates and non-blocking calls (esp_timer start/stop, pushInputEvent).
    std::mutex            m_stateMutex;
    Core::InputDescriptor m_btnState{};
    uint8_t               m_pendingClicks = 0;
    uint8_t               m_holdLevel     = 0;

    esp_timer_handle_t m_clickTimer = nullptr;
    esp_timer_handle_t m_holdTimer  = nullptr;
};

} // namespace InertialSaber::System::Adapters
