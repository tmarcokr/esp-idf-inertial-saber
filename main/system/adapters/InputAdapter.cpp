#include "InputAdapter.hpp"
#include "system/config/HardwareConfig.hpp"
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
    m_mainButton.onLongPress(Config::HardwareConfig::kHoldTickMs,
                             [this]() { onFirstHoldTick(); });

    ESP_LOGI(TAG, "Input Adapter started (GPIO %d, click_window=%u ms, hold_tick=%u ms)",
             static_cast<int>(Config::HardwareConfig::kMainBtn),
             static_cast<unsigned int>(Config::HardwareConfig::kClickWindowMs),
             static_cast<unsigned int>(Config::HardwareConfig::kHoldTickMs));
    return ESP_OK;
}

void InputAdapter::onPressDown() {
    const uint32_t now = esp_timer_get_time() / 1000;

    m_btnState.previous          = m_btnState.current;
    m_btnState.current           = Core::InputDescriptor::State::PRESSED;
    m_btnState.lastTransition_ms = now;

    m_pendingClicks.fetch_add(1, std::memory_order_relaxed);

    esp_timer_stop(m_clickTimer);
    esp_timer_start_once(m_clickTimer,
                         static_cast<uint64_t>(Config::HardwareConfig::kClickWindowMs) * 1000ULL);

    m_btnState.gesture = Core::InputDescriptor::Gesture::NONE;
    m_bus.pushInputEvent(Config::HardwareConfig::kMainBtnInputId, m_btnState);
    m_btnState.gesture = Core::InputDescriptor::Gesture::NONE;
}

void InputAdapter::onPressUp() {
    const uint32_t now = esp_timer_get_time() / 1000;

    esp_timer_stop(m_holdTimer);
    m_holdLevel.store(0, std::memory_order_relaxed);

    m_btnState.previous          = m_btnState.current;
    m_btnState.current           = Core::InputDescriptor::State::RELEASED;
    m_btnState.holdDuration_ms   = now - m_btnState.lastTransition_ms;
    m_btnState.lastTransition_ms = now;
    m_btnState.holdLevel         = 0;

    m_btnState.gesture = Core::InputDescriptor::Gesture::NONE;
    m_bus.pushInputEvent(Config::HardwareConfig::kMainBtnInputId, m_btnState);
    m_btnState.gesture = Core::InputDescriptor::Gesture::NONE;
}

void InputAdapter::onFirstHoldTick() {
    esp_timer_stop(m_clickTimer);
    m_pendingClicks.store(0, std::memory_order_relaxed);

    resolveHoldTick();
    esp_timer_start_periodic(m_holdTimer,
                             static_cast<uint64_t>(Config::HardwareConfig::kHoldTickMs) * 1000ULL);
}

void InputAdapter::resolveClickGesture() {
    const uint8_t count = m_pendingClicks.exchange(0, std::memory_order_relaxed);
    if (count == 0) return;

    using Gesture = Core::InputDescriptor::Gesture;
    m_btnState.pressCount = count;
    m_btnState.gesture    = Gesture::CLICK;

    m_bus.pushInputEvent(Config::HardwareConfig::kMainBtnInputId, m_btnState);
    m_btnState.gesture    = Gesture::NONE;
    m_btnState.pressCount = 0;

    ESP_LOGD(TAG, "Gesture resolved: CLICK x%u", static_cast<unsigned>(count));
}

void InputAdapter::resolveHoldTick() {
    const uint8_t level = m_holdLevel.fetch_add(1, std::memory_order_relaxed) + 1;

    m_btnState.current         = Core::InputDescriptor::State::HELD;
    m_btnState.holdDuration_ms = level * Config::HardwareConfig::kHoldTickMs;
    m_btnState.holdLevel       = level;
    m_btnState.gesture         = Core::InputDescriptor::Gesture::HOLD_TICK;

    m_bus.pushInputEvent(Config::HardwareConfig::kMainBtnInputId, m_btnState);
    m_btnState.gesture = Core::InputDescriptor::Gesture::NONE;

    ESP_LOGD(TAG, "Gesture resolved: HOLD_TICK level=%u (%u ms)",
             static_cast<unsigned>(level),
             static_cast<unsigned>(level * Config::HardwareConfig::kHoldTickMs));
}

/*static*/ void InputAdapter::clickTimerCallback(void* arg) {
    static_cast<InputAdapter*>(arg)->resolveClickGesture();
}

/*static*/ void InputAdapter::holdTimerCallback(void* arg) {
    static_cast<InputAdapter*>(arg)->resolveHoldTick();
}

} // namespace InertialSaber::System::Adapters
