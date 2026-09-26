#include "KineticImpactEffect.hpp"
#include "AudioEngine.hpp"
#include "overlays/BladeClashFlash.hpp"
#include "Engine.hpp"
#include "PowerToggleEffect.hpp"
#include "profiles/inertial/InertialDefinition.hpp"
#include "core/SaberDataPacket.hpp"

#include "esp_log.h"
#include "esp_random.h"

#include <algorithm>

namespace InertialSaber::Effects {

static constexpr const char *TAG = "KineticImpact";

KineticImpactEffect::KineticImpactEffect(
    PowerToggleEffect &power,
    Espressif::Wrappers::Audio::AudioEngine &audio,
    Espressif::Wrappers::SmartLed::Engine &ledEngine,
    const InertialSaber::Profiles::Inertial::InertialDefinition &definition)
    : InertialEffect(2)
    , m_power(power)
    , m_audio(audio)
    , m_ledEngine(ledEngine)
    , m_def(definition)
{}

bool KineticImpactEffect::test(const Core::SaberDataPacket &packet) {
    if (!m_power.isIgnited()) {
        clearKineticEnergyWindow();
        return false;
    }
    return detectClash(packet);
}

void KineticImpactEffect::clearKineticEnergyWindow() {
    m_kineticEnergyWindow.fill(0.0f);
}

bool KineticImpactEffect::detectClash(const Core::SaberDataPacket &packet) {
    m_kineticEnergyWindow[m_windowIdx] = packet.kineticEnergy;
    m_windowIdx = (m_windowIdx + 1) % m_kineticEnergyWindow.size();

    float peakKineticEnergyG = 0.0f;
    for (float val : m_kineticEnergyWindow) {
        if (val > peakKineticEnergyG) {
            peakKineticEnergyG = val;
        }
    }

    float decelerationG = peakKineticEnergyG - packet.kineticEnergy;

    if (decelerationG > m_def.clashThresholdG && (packet.timestampMs - m_lastClashTimeMs) > 500) {
        m_lastClashTimeMs = packet.timestampMs;
        return true;
    }

    return false;
}

void KineticImpactEffect::run() {
    const uint8_t index = static_cast<uint8_t>(
        esp_random() % std::max<uint8_t>(m_def.fontClashCount, 1));
    const std::string path = buildPath("clsh/clsh", index);

    m_audio.play(path, false, 16384);
    m_ledEngine.pushOverlay(std::make_unique<BladeClashFlash>(
        m_ledEngine.numLeds(), m_def.bladeBaseHue, m_def.clashDurationMs));

    ESP_LOGI(TAG, "Clash triggered: %s (G drop threshold: %.2f)", path.c_str(), m_def.clashThresholdG);
}

std::string KineticImpactEffect::buildPath(const char *subAndPrefix,
                                           uint8_t index) const {
    return std::string("/sdcard/") + m_def.profileRoot + subAndPrefix +
           std::to_string(index + 1) + ".wav";
}

} // namespace InertialSaber::Effects
