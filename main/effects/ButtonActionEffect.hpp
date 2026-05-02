#pragma once

#include "InertialEffect.hpp"

#include "esp_log.h"

namespace InertialSaber::Effects {

/**
 * @brief System-level effect that logs button state transitions.
 *
 * Priority 3 (System). Fires on the rising edge of a button press
 * for a configurable input ID. Validates that input events flow
 * through the SaberAction Bus correctly.
 */
class ButtonActionEffect final : public Core::InertialEffect {
public:
    explicit ButtonActionEffect(uint8_t inputId = 0) : m_inputId(inputId) {
        Priority = 3;
    }

    bool Test(const Core::SaberDataPacket& packet) override {
        if (m_inputId >= Core::Platform::kMaxInputs) {
            return false;
        }
        const auto& input = packet.inputs[m_inputId];
        using State = Core::InputDescriptor::State;
        return input.current == State::PRESSED && input.previous != State::PRESSED;
    }

    void Run() override {
        ESP_LOGI(TAG, "Button %d activated", m_inputId);
    }

private:
    static constexpr const char* TAG = "ButtonAction";
    uint8_t m_inputId;
};

} // namespace InertialSaber::Effects
