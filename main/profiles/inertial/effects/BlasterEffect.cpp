#include "BlasterEffect.hpp"
#include "diagnostics/Metrics.hpp"
#include "AudioEngine.hpp"
#include "AudioLevels.hpp"
#include "overlays/BladeBlasterBlock.hpp"
#include "Engine.hpp"
#include "profiles/inertial/InertialDefinition.hpp"
#include "profiles/PowerStateMachine.hpp"
#include "profiles/SoundFont.hpp"
#include "core/SaberDataPacket.hpp"

#include "esp_log.h"

#include <string>

namespace InertialSaber::Effects {

static constexpr const char* TAG = "BlasterEffect";

BlasterEffect::BlasterEffect(
    const Profiles::PowerStateMachine& power, Espressif::Wrappers::Audio::AudioEngine& audio,
    Espressif::Wrappers::SmartLed::Engine& ledEngine,
    const InertialSaber::Profiles::Inertial::InertialDefinition& definition,
    const Profiles::SoundFont& font, uint8_t buttonId)
    : InertialEffect(2)
    , m_power(power)
    , m_audio(audio)
    , m_ledEngine(ledEngine)
    , m_def(definition)
    , m_font(font)
    , m_buttonId(buttonId) {}

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
    SABER_METRIC_SCOPE(Diagnostics::Metric::RunBlaster);
    const std::string path = m_font.randomPath(Profiles::FontCategory::Blaster);

    m_audio.play(path, false, kFullVolume);
    if (!m_ledEngine.pushOverlay(std::make_unique<BladeBlasterBlock>(
            m_ledEngine.numLeds(), m_def.blasterLedCount, m_def.blasterDurationMs))) {
        SABER_METRIC_COUNT(Diagnostics::Counter::OverlaysDropped);
        ESP_LOGW(TAG, "Blaster overlay dropped: no free overlay slot");
    }

    ESP_LOGD(TAG, "Blaster block triggered: %s", path.c_str());
}

} // namespace InertialSaber::Effects
