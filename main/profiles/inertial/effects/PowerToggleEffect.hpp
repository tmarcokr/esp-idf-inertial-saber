#pragma once

#include "interfaces/InertialEffect.hpp"
#include "InertialSwingEffect.hpp"
#include "InertialLightEffect.hpp"

#include "esp_log.h"

namespace InertialSaber::Effects {

/**
 * @brief System-level effect that toggles InertialSwing on button click.
 *
 * Priority 3 (System). Fires on a single short press (< 300ms) of the
 * configured button. Activates or deactivates the InertialSwing engine.
 * This is a temporary bridge until the Phase 3 Power State Machine
 * replaces it with proper ignition/retraction sequencing.
 */
class PowerToggleEffect final : public Core::InertialEffect {
public:
  PowerToggleEffect(InertialSwingEffect &swing, InertialLightEffect &light, uint8_t buttonId)
      : m_swing(swing), m_light(light), m_buttonId(buttonId) {
    Priority = 3;
  }

  bool Test(const Core::SaberDataPacket &packet) override {
    if (m_buttonId >= Core::Platform::kMaxInputs)
      return false;

    const auto &input = packet.inputs[m_buttonId];
    using State = Core::InputDescriptor::State;

    bool clicked =
        (input.current == State::RELEASED && input.previous != State::IDLE &&
         input.holdDuration_ms < 300);

    if (clicked) {
      m_isOn = !m_isOn; // Toggle internal state
      return true;
    }

    return false;
  }

  void Run() override {
    if (m_isOn) {
      m_swing.activate();
      m_light.activate();
      ESP_LOGI(TAG, "Saber ON");
    } else {
      m_swing.deactivate();
      m_light.deactivate();
      ESP_LOGI(TAG, "Saber OFF");
    }
  }

private:
  static constexpr const char *TAG = "PowerToggle";
  InertialSwingEffect &m_swing;
  InertialLightEffect &m_light;
  uint8_t m_buttonId;
  bool m_isOn = false;
};

} // namespace InertialSaber::Effects
