#include "PowerToggleEffect.hpp"
#include "interfaces/InertialProfile.hpp"
#include "AudioEngine.hpp"
#include "../overlays/BladeIgniteSweep.hpp"
#include "../overlays/BladeRetractSweep.hpp"
#include "Engine.hpp"
#include "InertialLightEffect.hpp"
#include "InertialSwingEffect.hpp"
#include "models/InertialDefinition.hpp"
#include "models/SaberDataPacket.hpp"

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"

#include <algorithm>
#include <cinttypes>
#include <memory>

namespace InertialSaber::Effects {

static constexpr const char *TAG = "PowerToggle";
static constexpr uint32_t kSwingPreStartMs = 100;

static uint32_t nowMs() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
}

PowerToggleEffect::PowerToggleEffect(
    Core::InertialProfile &profile,
    InertialSwingEffect &swing, InertialLightEffect &light,
    Espressif::Wrappers::Audio::AudioEngine &audio,
    Espressif::Wrappers::SmartLed::Engine &ledEngine,
    const Core::InertialDefinition &definition, uint8_t buttonId)
    : m_profile(profile), m_swing(swing), m_light(light), m_audio(audio), m_ledEngine(ledEngine),
      m_def(definition), m_buttonId(buttonId) {
  Priority = 1;
}

bool PowerToggleEffect::Test(const Core::SaberDataPacket& packet) {
    if (m_buttonId >= Core::Platform::kMaxInputs) {
        return false;
    }

    using ProfileState = Core::InertialProfile::PowerState;
    using Gesture      = Core::InputDescriptor::Gesture;

    const auto state  = m_profile.getPowerState();
    const auto& input = packet.inputs[m_buttonId];

    if (state == ProfileState::IGNITING || state == ProfileState::RETRACTING) {
        return true;
    }

    if (state == ProfileState::RETRACTED &&
        input.gesture == Gesture::CLICK && input.pressCount == 1) {
        m_pendingTransition = true;
        return true;
    }

    if (state == ProfileState::IGNITED &&
        input.gesture == Gesture::CLICK && input.pressCount == 2) {
        m_pendingTransition = true;
        return true;
    }

    return false;
}

void PowerToggleEffect::Run() {
  using ProfileState = Core::InertialProfile::PowerState;
  switch (m_profile.getPowerState()) {
  case ProfileState::RETRACTED:
    if (m_pendingTransition) {
      m_pendingTransition = false;
      beginIgnition();
    }
    break;

  case ProfileState::IGNITING:
    tickIgnition();
    break;

  case ProfileState::IGNITED:
    if (m_pendingTransition) {
      m_pendingTransition = false;
      beginRetraction();
    }
    break;

  case ProfileState::RETRACTING:
    tickRetraction();
    break;
  }
}

void PowerToggleEffect::beginIgnition() {
  const uint8_t index = static_cast<uint8_t>(
      esp_random() % std::max<uint8_t>(m_def.fontInCount, 1));
  const std::string path = buildPath("in/in", index);

  m_audio.play(path, false, 16384);
  m_ledEngine.pushOverlay(std::make_unique<BladeIgniteSweep>(
      m_ledEngine.numLeds(), m_def.bladeBaseHue, m_def.ignitionDurationMs));

  m_sequenceStartMs = nowMs();
  m_enginesStarted = false;
  m_profile.setPowerState(Core::InertialProfile::PowerState::IGNITING);

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
    m_profile.setPowerState(Core::InertialProfile::PowerState::IGNITED);
    ESP_LOGI(TAG, "Saber ON");
  }
}

void PowerToggleEffect::beginRetraction() {
  m_swing.deactivate();
  m_light.deactivate();

  const uint8_t index = static_cast<uint8_t>(
      esp_random() % std::max<uint8_t>(m_def.fontOutCount, 1));
  const std::string path = buildPath("out/out", index);

  m_audio.play(path, false, 16384);
  m_ledEngine.pushOverlay(std::make_unique<BladeRetractSweep>(
      m_ledEngine.numLeds(), m_def.bladeBaseHue, m_def.retractionDurationMs));

  m_sequenceStartMs = nowMs();
  m_profile.setPowerState(Core::InertialProfile::PowerState::RETRACTING);

  ESP_LOGI(TAG, "Retraction started — %s (%" PRIu32 " ms)", path.c_str(), m_def.retractionDurationMs);
}

void PowerToggleEffect::tickRetraction() {
  if ((nowMs() - m_sequenceStartMs) >= m_def.retractionDurationMs) {
    m_profile.setPowerState(Core::InertialProfile::PowerState::RETRACTED);
    ESP_LOGI(TAG, "Saber OFF");
  }
}

std::string PowerToggleEffect::buildPath(const char *subAndPrefix,
                                         uint8_t index) const {
  return std::string("/sdcard/") + m_def.profileRoot + subAndPrefix +
         std::to_string(index + 1) + ".wav";
}

bool PowerToggleEffect::isIgnited() const {
  return m_profile.getPowerState() == Core::InertialProfile::PowerState::IGNITED;
}

bool PowerToggleEffect::isRetracted() const {
  return m_profile.getPowerState() == Core::InertialProfile::PowerState::RETRACTED;
}

} // namespace InertialSaber::Effects
