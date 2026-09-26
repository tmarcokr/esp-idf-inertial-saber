#include "BlasterEffect.hpp"
#include "AudioEngine.hpp"
#include "AudioLevels.hpp"
#include "overlays/BladeBlasterBlock.hpp"
#include "Engine.hpp"
#include "PowerToggleEffect.hpp"
#include "profiles/inertial/InertialDefinition.hpp"
#include "profiles/SoundFont.hpp"
#include "core/SaberDataPacket.hpp"

#include "esp_log.h"

#include <string>

namespace InertialSaber::Effects {

static constexpr const char *TAG = "BlasterEffect";

BlasterEffect::BlasterEffect(
    PowerToggleEffect &power,
    Espressif::Wrappers::Audio::AudioEngine &audio,
    Espressif::Wrappers::SmartLed::Engine &ledEngine,
    const InertialSaber::Profiles::Inertial::InertialDefinition &definition,
    const Profiles::SoundFont &font,
    uint8_t buttonId)
    : InertialEffect(2)
    , m_power(power)
    , m_audio(audio)
    , m_ledEngine(ledEngine)
    , m_def(definition)
    , m_font(font)
    , m_buttonId(buttonId)
{}

bool BlasterEffect::test(const Core::SaberDataPacket& packet) {
    if (!m_power.isIgnited()) {
        return false;
    }
    if (m_buttonId >= Core::kMaxInputs) {
        return false;
    }

    const auto& input = packet.inputs[m_buttonId];
    using Gesture = Core::InputDescriptor::Gesture;
    return input.gesture == Gesture::Click && input.pressCount == 1;
}

void BlasterEffect::run() {
  const std::string path = m_font.randomPath(Profiles::FontCategory::Blaster);

  m_audio.play(path, false, kFullVolume);
  m_ledEngine.pushOverlay(std::make_unique<BladeBlasterBlock>(
      m_ledEngine.numLeds(), m_def.blasterLedCount, m_def.blasterDurationMs));

  ESP_LOGI(TAG, "Blaster block triggered: %s", path.c_str());
}

} // namespace InertialSaber::Effects
