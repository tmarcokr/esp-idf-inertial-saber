#include "PowerToggleEffect.hpp"
#include "profiles/PowerStateMachine.hpp"
#include "AudioEngine.hpp"
#include "AudioLevels.hpp"
#include "overlays/BladeIgniteSweep.hpp"
#include "overlays/BladeRetractSweep.hpp"
#include "Engine.hpp"
#include "InertialLightEffect.hpp"
#include "InertialSwingEffect.hpp"
#include "profiles/inertial/InertialDefinition.hpp"
#include "profiles/SoundFont.hpp"
#include "core/SaberDataPacket.hpp"

#include "esp_log.h"
#include "esp_timer.h"

#include <cinttypes>
#include <memory>
#include <string>

namespace InertialSaber::Effects {

static constexpr const char *TAG = "PowerToggle";
static constexpr uint32_t kSwingPreStartMs = 100;

static uint32_t nowMs() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
}

PowerToggleEffect::PowerToggleEffect(
    Profiles::PowerStateMachine &power,
    InertialSwingEffect &swing, InertialLightEffect &light,
    Espressif::Wrappers::Audio::AudioEngine &audio,
    Espressif::Wrappers::SmartLed::Engine &ledEngine,
    const InertialSaber::Profiles::Inertial::InertialDefinition &definition,
    const Profiles::SoundFont &font, uint8_t buttonId)
    : InertialEffect(1), m_power(power), m_swing(swing), m_light(light), m_audio(audio), m_ledEngine(ledEngine),
      m_def(definition), m_font(font), m_buttonId(buttonId) {}

bool PowerToggleEffect::test(const Core::SaberDataPacket& packet) {
    if (m_buttonId >= Core::kMaxInputs) {
        return false;
    }

    using State        = Profiles::PowerStateMachine::State;
    using Gesture      = Core::InputDescriptor::Gesture;

    const auto state  = m_power.state();
    const auto& input = packet.inputs[m_buttonId];

    if (state == State::Igniting || state == State::Retracting) {
        return true;
    }

    if (state == State::Retracted &&
        input.gesture == Gesture::Click && input.pressCount == 1) {
        m_pendingTransition = true;
        return true;
    }

    if (state == State::Ignited &&
        input.gesture == Gesture::Click && input.pressCount == 2) {
        m_pendingTransition = true;
        return true;
    }

    return false;
}

void PowerToggleEffect::run() {
  using State = Profiles::PowerStateMachine::State;
  switch (m_power.state()) {
  case State::Retracted:
    if (m_pendingTransition) {
      m_pendingTransition = false;
      beginIgnition();
    }
    break;

  case State::Igniting:
    tickIgnition();
    break;

  case State::Ignited:
    if (m_pendingTransition) {
      m_pendingTransition = false;
      beginRetraction();
    }
    break;

  case State::Retracting:
    tickRetraction();
    break;

  case State::Locked:
  case State::Faulted:
    break;
  }
}

void PowerToggleEffect::beginIgnition() {
  const std::string path = m_font.randomPath(Profiles::FontCategory::Ignition);

  m_audio.play(path, false, kFullVolume);
  m_ledEngine.pushOverlay(std::make_unique<BladeIgniteSweep>(
      m_ledEngine.numLeds(), m_def.bladeBaseHue, m_def.ignitionDurationMs));

  m_sequenceStartMs = nowMs();
  m_enginesStarted = false;
  m_power.handle(Profiles::PowerStateMachine::Event::IgniteRequested);

  ESP_LOGI(TAG, "Ignition started — %s (%" PRIu32 " ms)", path.c_str(), m_def.ignitionDurationMs);
}

void PowerToggleEffect::tickIgnition() {
  const uint32_t elapsed = nowMs() - m_sequenceStartMs;

  if (!m_enginesStarted &&
      elapsed >= (m_def.ignitionDurationMs - kSwingPreStartMs)) {
    m_swing.activate();
    m_light.activate();
    m_enginesStarted = true;
    ESP_LOGI(TAG, "Engines activated at +%" PRIu32 " ms", elapsed);
  }

  if (elapsed >= m_def.ignitionDurationMs) {
    m_power.handle(Profiles::PowerStateMachine::Event::IgnitionElapsed);
    ESP_LOGI(TAG, "Saber ON");
  }
}

void PowerToggleEffect::beginRetraction() {
  m_swing.deactivate();
  m_light.deactivate();

  const std::string path = m_font.randomPath(Profiles::FontCategory::Retraction);

  m_audio.play(path, false, kRetractionVolume);
  m_ledEngine.pushOverlay(std::make_unique<BladeRetractSweep>(
      m_ledEngine.numLeds(), m_def.bladeBaseHue, m_def.retractionDurationMs));

  m_sequenceStartMs = nowMs();
  m_power.handle(Profiles::PowerStateMachine::Event::RetractRequested);

  ESP_LOGI(TAG, "Retraction started — %s (%" PRIu32 " ms)", path.c_str(), m_def.retractionDurationMs);
}

void PowerToggleEffect::tickRetraction() {
  if ((nowMs() - m_sequenceStartMs) >= m_def.retractionDurationMs) {
    m_power.handle(Profiles::PowerStateMachine::Event::RetractionElapsed);
    ESP_LOGI(TAG, "Saber OFF");
  }
}

} // namespace InertialSaber::Effects
