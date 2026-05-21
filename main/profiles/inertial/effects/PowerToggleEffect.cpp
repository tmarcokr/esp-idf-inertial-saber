#include "PowerToggleEffect.hpp"
#include "AudioEngine.hpp"
#include "BladeIgniteSweep.hpp"
#include "BladeRetractSweep.hpp"
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
    InertialSwingEffect &swing, InertialLightEffect &light,
    Espressif::Wrappers::Audio::AudioEngine &audio,
    Espressif::Wrappers::SmartLed::Engine &ledEngine,
    const Core::InertialDefinition &definition, uint8_t buttonId)
    : m_swing(swing), m_light(light), m_audio(audio), m_ledEngine(ledEngine),
      m_def(definition), m_buttonId(buttonId) {
  Priority = 3;
}

bool PowerToggleEffect::Test(const Core::SaberDataPacket &packet) {
  if (m_buttonId >= Core::Platform::kMaxInputs) {
    return false;
  }

  if (m_state == State::IGNITING || m_state == State::RETRACTING) {
    return true;
  }

  const auto &input = packet.inputs[m_buttonId];
  using InputState = Core::InputDescriptor::State;

  const bool clicked =
      (input.current == InputState::RELEASED &&
       input.previous != InputState::IDLE && input.holdDuration_ms < 300);

  if (clicked) {
    m_pendingTransition = true;
    return true;
  }
  return false;
}

void PowerToggleEffect::Run() {
  switch (m_state) {
  case State::IDLE_OFF:
    if (m_pendingTransition) {
      m_pendingTransition = false;
      beginIgnition();
    }
    break;

  case State::IGNITING:
    tickIgnition();
    break;

  case State::IDLE_ON:
    if (m_pendingTransition) {
      m_pendingTransition = false;
      beginRetraction();
    }
    break;

  case State::RETRACTING:
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
  m_state = State::IGNITING;

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
    m_state = State::IDLE_ON;
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
  m_state = State::RETRACTING;

  ESP_LOGI(TAG, "Retraction started — %s (%" PRIu32 " ms)", path.c_str(), m_def.retractionDurationMs);
}

void PowerToggleEffect::tickRetraction() {
  if ((nowMs() - m_sequenceStartMs) >= m_def.retractionDurationMs) {
    m_state = State::IDLE_OFF;
    ESP_LOGI(TAG, "Saber OFF");
  }
}

std::string PowerToggleEffect::buildPath(const char *subAndPrefix,
                                         uint8_t index) const {
  return std::string("/sdcard/") + m_def.profileRoot + subAndPrefix +
         std::to_string(index + 1) + ".wav";
}

} // namespace InertialSaber::Effects
