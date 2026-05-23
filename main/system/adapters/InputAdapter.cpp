#include "InputAdapter.hpp"
#include "esp_log.h"
#include "esp_timer.h"

namespace InertialSaber::System::Adapters {

static constexpr const char* TAG = "InputAdapter";

InputAdapter::InputAdapter(Core::SaberActionBus& bus, Espressif::Wrappers::GpioButton& button)
    : m_bus(bus), m_mainButton(button) {}

esp_err_t InputAdapter::start() {
    m_mainButton.onEvent(Espressif::Wrappers::ButtonEvent::PressDown, [this]() {
        m_btnState.previous = m_btnState.current;
        m_btnState.current = Core::InputDescriptor::State::PRESSED;
        m_btnState.lastTransition_ms = esp_timer_get_time() / 1000;
        m_btnState.pressCount++;
        m_bus.pushInputEvent(Config::HardwareConfig::kMainBtnInputId, m_btnState);
    });

    m_mainButton.onEvent(Espressif::Wrappers::ButtonEvent::PressUp, [this]() {
        uint32_t now = esp_timer_get_time() / 1000;
        m_btnState.previous = m_btnState.current;
        m_btnState.current = Core::InputDescriptor::State::RELEASED;
        m_btnState.holdDuration_ms = now - m_btnState.lastTransition_ms;
        m_btnState.lastTransition_ms = now;
        m_bus.pushInputEvent(Config::HardwareConfig::kMainBtnInputId, m_btnState);
    });

    m_mainButton.onLongPress(500, [this]() {
        m_btnState.previous = m_btnState.current;
        m_btnState.current = Core::InputDescriptor::State::HELD;
        m_btnState.holdDuration_ms = 500;
        m_bus.pushInputEvent(Config::HardwareConfig::kMainBtnInputId, m_btnState);
    });

    ESP_LOGI(TAG, "Input Adapter started successfully (GPIO %d)", Config::HardwareConfig::kMainBtn);
    return ESP_OK;
}

} // namespace InertialSaber::System::Adapters
