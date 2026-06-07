#pragma once

#include "core/bus/SaberActionBus.hpp"
#include "core/models/SaberDataPacket.hpp"
#include "GpioButton.hpp"
#include "esp_timer.h"

#include <atomic>
#include <cstdint>

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

    static void clickTimerCallback(void* arg);
    static void holdTimerCallback(void* arg);

    Core::SaberActionBus&            m_bus;
    Espressif::Wrappers::GpioButton& m_mainButton;
    Core::InputDescriptor            m_btnState{};

    // Thread safety:
    // m_pendingClicks - std::atomic<uint8_t>: written by GpioButton poll task,
    //                   read/reset by click esp_timer task.
    // m_holdLevel     - std::atomic<uint8_t>: written by hold esp_timer task,
    //                   reset by GpioButton poll task (PressUp).
    std::atomic<uint8_t> m_pendingClicks{0};
    std::atomic<uint8_t> m_holdLevel{0};

    esp_timer_handle_t m_clickTimer = nullptr;
    esp_timer_handle_t m_holdTimer  = nullptr;
};

} // namespace InertialSaber::System::Adapters
