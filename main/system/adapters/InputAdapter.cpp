#include "InputAdapter.hpp"
#include "system/board/Board.hpp"
#include "system/hardware/HardwareConfig.hpp"
#include "esp_log.h"
#include "esp_timer.h"

namespace InertialSaber::System::Adapters {

static constexpr const char* TAG = "InputAdapter";

InputAdapter::InputAdapter(Core::SaberActionBus& bus,
                           Espressif::Wrappers::GpioButton& button)
    : m_bus(bus), m_mainButton(button) {}

InputAdapter::~InputAdapter() {
    if (m_clickTimer) {
        esp_timer_stop(m_clickTimer);
        esp_timer_delete(m_clickTimer);
    }
    if (m_holdTimer) {
        esp_timer_stop(m_holdTimer);
        esp_timer_delete(m_holdTimer);
    }
}

esp_err_t InputAdapter::start() {
    const esp_timer_create_args_t clickArgs = {
        .callback              = &InputAdapter::clickTimerCallback,
        .arg                   = this,
        .dispatch_method       = ESP_TIMER_TASK,
        .name                  = "click_window",
        .skip_unhandled_events = true,
    };
    esp_err_t err = esp_timer_create(&clickArgs, &m_clickTimer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create click timer: %s", esp_err_to_name(err));
        return err;
    }

    const esp_timer_create_args_t holdArgs = {
        .callback              = &InputAdapter::holdTimerCallback,
        .arg                   = this,
        .dispatch_method       = ESP_TIMER_TASK,
        .name                  = "hold_tick",
        .skip_unhandled_events = true,
    };
    err = esp_timer_create(&holdArgs, &m_holdTimer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create hold timer: %s", esp_err_to_name(err));
        return err;
    }

    m_mainButton.onEvent(Espressif::Wrappers::ButtonEvent::PressDown,
                         [this]() { onPressDown(); });
    m_mainButton.onEvent(Espressif::Wrappers::ButtonEvent::PressUp,
                         [this]() { onPressUp(); });
    m_mainButton.onLongPress(Hardware::HardwareConfig::kHoldTickMs,
                             [this]() { onFirstHoldTick(); });

    ESP_LOGI(TAG, "Input Adapter started (GPIO %d, click_window=%u ms, hold_tick=%u ms)",
             static_cast<int>(Board::kPins.mainButton),
             static_cast<unsigned int>(Hardware::HardwareConfig::kClickWindowMs),
             static_cast<unsigned int>(Hardware::HardwareConfig::kHoldTickMs));
    return ESP_OK;
}

void InputAdapter::onPressDown() {
    const uint32_t now = esp_timer_get_time() / 1000;
    std::lock_guard lock(m_stateMutex);

    m_btnState.previous         = m_btnState.current;
    m_btnState.current          = Core::InputDescriptor::State::Pressed;
    m_btnState.lastTransitionMs = now;

    ++m_pendingClicks;

    esp_timer_stop(m_clickTimer);
    esp_timer_start_once(m_clickTimer,
                         static_cast<uint64_t>(Hardware::HardwareConfig::kClickWindowMs) * 1000ULL);

    m_btnState.gesture = Core::InputDescriptor::Gesture::None;
    m_bus.pushInputEvent(Core::kMainButtonInputId, m_btnState);
    m_btnState.gesture = Core::InputDescriptor::Gesture::None;
}

void InputAdapter::onPressUp() {
    const uint32_t now = esp_timer_get_time() / 1000;
    std::lock_guard lock(m_stateMutex);

    esp_timer_stop(m_holdTimer);
    m_holdLevel = 0;

    m_btnState.previous         = m_btnState.current;
    m_btnState.current          = Core::InputDescriptor::State::Released;
    m_btnState.holdDurationMs   = now - m_btnState.lastTransitionMs;
    m_btnState.lastTransitionMs = now;
    m_btnState.holdLevel        = 0;

    m_btnState.gesture = Core::InputDescriptor::Gesture::None;
    m_bus.pushInputEvent(Core::kMainButtonInputId, m_btnState);
    m_btnState.gesture = Core::InputDescriptor::Gesture::None;
}

void InputAdapter::onFirstHoldTick() {
    uint8_t level = 0;
    {
        std::lock_guard lock(m_stateMutex);

        esp_timer_stop(m_clickTimer);
        m_pendingClicks = 0;

        level = emitHoldTickLocked();
        esp_timer_start_periodic(m_holdTimer,
                                 static_cast<uint64_t>(Hardware::HardwareConfig::kHoldTickMs) * 1000ULL);
    }

    ESP_LOGD(TAG, "Gesture resolved: HoldTick level=%u (%u ms)",
             static_cast<unsigned>(level),
             static_cast<unsigned>(level * Hardware::HardwareConfig::kHoldTickMs));
}

void InputAdapter::resolveClickGesture() {
    uint8_t count = 0;
    {
        std::lock_guard lock(m_stateMutex);

        count           = m_pendingClicks;
        m_pendingClicks = 0;
        if (count == 0) return;

        using Gesture = Core::InputDescriptor::Gesture;
        m_btnState.pressCount = count;
        m_btnState.gesture    = Gesture::Click;

        m_bus.pushInputEvent(Core::kMainButtonInputId, m_btnState);
        m_btnState.gesture    = Gesture::None;
        m_btnState.pressCount = 0;
    }

    ESP_LOGD(TAG, "Gesture resolved: Click x%u", static_cast<unsigned>(count));
}

void InputAdapter::resolveHoldTick() {
    uint8_t level = 0;
    {
        std::lock_guard lock(m_stateMutex);

        // Warning: a tick dispatched just before onPressUp() stopped the timer belongs to a finished hold.
        if (m_btnState.current != Core::InputDescriptor::State::Held) return;

        level = emitHoldTickLocked();
    }

    ESP_LOGD(TAG, "Gesture resolved: HoldTick level=%u (%u ms)",
             static_cast<unsigned>(level),
             static_cast<unsigned>(level * Hardware::HardwareConfig::kHoldTickMs));
}

uint8_t InputAdapter::emitHoldTickLocked() {
    const uint8_t level = ++m_holdLevel;

    m_btnState.current        = Core::InputDescriptor::State::Held;
    m_btnState.holdDurationMs = level * Hardware::HardwareConfig::kHoldTickMs;
    m_btnState.holdLevel      = level;
    m_btnState.gesture        = Core::InputDescriptor::Gesture::HoldTick;

    m_bus.pushInputEvent(Core::kMainButtonInputId, m_btnState);
    m_btnState.gesture = Core::InputDescriptor::Gesture::None;

    return level;
}

/*static*/ void InputAdapter::clickTimerCallback(void* arg) {
    static_cast<InputAdapter*>(arg)->resolveClickGesture();
}

/*static*/ void InputAdapter::holdTimerCallback(void* arg) {
    static_cast<InputAdapter*>(arg)->resolveHoldTick();
}

} // namespace InertialSaber::System::Adapters
