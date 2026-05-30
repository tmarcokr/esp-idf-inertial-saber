#include "BlasterEffect.hpp"
#include "AudioEngine.hpp"
#include "BladeBlasterBlock.hpp"
#include "Engine.hpp"
#include "PowerToggleEffect.hpp"
#include "models/InertialDefinition.hpp"
#include "models/SaberDataPacket.hpp"

#include "esp_log.h"
#include "esp_random.h"

#include <algorithm>

namespace InertialSaber::Effects {

static constexpr const char *TAG = "BlasterEffect";

BlasterEffect::BlasterEffect(
    PowerToggleEffect &power,
    Espressif::Wrappers::Audio::AudioEngine &audio,
    Espressif::Wrappers::SmartLed::Engine &ledEngine,
    const Core::InertialDefinition &definition,
    uint8_t buttonId)
    : m_power(power)
    , m_audio(audio)
    , m_ledEngine(ledEngine)
    , m_def(definition)
    , m_buttonId(buttonId)
{
    Priority = 2;
}

bool BlasterEffect::Test(const Core::SaberDataPacket &packet) {
  if (!m_power.isIgnited()) {
    return false;
  }
  if (m_buttonId >= Core::Platform::kMaxInputs) {
    return false;
  }

  const auto &input = packet.inputs[m_buttonId];
  using InputState = Core::InputDescriptor::State;

  return (input.current == InputState::RELEASED &&
          input.previous != InputState::IDLE &&
          input.holdDuration_ms < 500);
}

void BlasterEffect::Run() {
  const uint8_t index = static_cast<uint8_t>(
      esp_random() % std::max<uint8_t>(m_def.fontBlasterCount, 1));
  const std::string path = buildPath("blst/blst", index);

  m_audio.play(path, false, 16384);
  m_ledEngine.pushOverlay(std::make_unique<BladeBlasterBlock>(
      m_ledEngine.numLeds(), m_def.blasterLedCount, m_def.blasterDurationMs));

  ESP_LOGI(TAG, "Blaster block triggered: %s", path.c_str());
}

std::string BlasterEffect::buildPath(const char *subAndPrefix,
                                     uint8_t index) const {
  return std::string("/sdcard/") + m_def.profileRoot + subAndPrefix +
         std::to_string(index + 1) + ".wav";
}

} // namespace InertialSaber::Effects
