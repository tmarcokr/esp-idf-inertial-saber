#include "KineticImpactEffect.hpp"
#include "diagnostics/Metrics.hpp"
#include "system/audio/AudioController.hpp"
#include "AudioLevels.hpp"
#include "overlays/BladeClashFlash.hpp"
#include "Engine.hpp"
#include "profiles/inertial/InertialDefinition.hpp"
#include "profiles/PowerStateMachine.hpp"
#include "profiles/SoundFont.hpp"
#include "core/SaberDataPacket.hpp"

#include "esp_log.h"

#include <algorithm>

namespace InertialSaber::Effects {

static constexpr const char* TAG = "KineticImpact";

KineticImpactEffect::KineticImpactEffect(
    const Profiles::PowerStateMachine& power, System::AudioController& audio,
    Espressif::Wrappers::SmartLed::Engine& ledEngine,
    const InertialSaber::Profiles::Inertial::InertialDefinition& definition,
    const Profiles::SoundFont& font)
    : InertialEffect(2)
    , m_power(power)
    , m_audio(audio)
    , m_ledEngine(ledEngine)
    , m_def(definition)
    , m_font(font) {}

bool KineticImpactEffect::test(const Core::SaberDataPacket& packet) {
    if (!m_power.isIgnited()) {
        clearKineticEnergyHistory();
        return false;
    }
    return detectClash(packet);
}

void KineticImpactEffect::clearKineticEnergyHistory() {
    m_historyNext = 0;
    m_historySize = 0;
}

void KineticImpactEffect::recordKineticEnergy(int64_t timestampUs, float kineticEnergyG) {
    m_kineticEnergyHistory[m_historyNext] = {timestampUs, kineticEnergyG};
    m_historyNext = (m_historyNext + 1) % m_kineticEnergyHistory.size();
    m_historySize = std::min(m_historySize + 1, m_kineticEnergyHistory.size());
}

float KineticImpactEffect::peakKineticEnergySince(int64_t oldestTimestampUs) const {
    const size_t capacity = m_kineticEnergyHistory.size();
    float peakKineticEnergyG = 0.0f;
    for (size_t age = 0; age < m_historySize; ++age) {
        const KineticEnergySample& sample =
            m_kineticEnergyHistory[(m_historyNext + capacity - 1 - age) % capacity];
        if (sample.timestampUs < oldestTimestampUs) {
            break;
        }
        peakKineticEnergyG = std::max(peakKineticEnergyG, sample.kineticEnergyG);
    }
    return peakKineticEnergyG;
}

bool KineticImpactEffect::detectClash(const Core::SaberDataPacket& packet) {
    const int64_t sampleTimestampUs = packet.motionTimestampUs;
    if (sampleTimestampUs == 0 || sampleTimestampUs == m_lastSampleTimestampUs) {
        return false;
    }
    m_lastSampleTimestampUs = sampleTimestampUs;
    recordKineticEnergy(sampleTimestampUs, packet.kineticEnergy);

    const float decelerationG =
        peakKineticEnergySince(sampleTimestampUs - kClashWindowUs) - packet.kineticEnergy;

    if (decelerationG > m_def.clashThresholdG &&
        (packet.timestampMs - m_lastClashTimeMs) > kClashDebounceMs) {
        m_previousClashTimeMs = m_lastClashTimeMs;
        m_lastClashTimeMs = packet.timestampMs;
        return true;
    }

    return false;
}

void KineticImpactEffect::run() {
    SABER_METRIC_SCOPE(Diagnostics::Metric::RunClash);
    SABER_METRIC_COUNT(Diagnostics::Counter::ClashDetections);
    if (m_previousClashTimeMs != 0 &&
        (m_lastClashTimeMs - m_previousClashTimeMs) < kClashRetriggerWindowMs) {
        SABER_METRIC_COUNT(Diagnostics::Counter::ClashRetriggerLt1s);
    }

    const System::AudioPath path = m_font.randomPath(Profiles::FontCategory::Clash);

    m_audio.playOneShot(path, kFullVolume);
    if (!m_ledEngine.pushOverlay(std::make_unique<BladeClashFlash>(
            m_ledEngine.numLeds(), m_def.bladeBaseHue, m_def.clashDurationMs))) {
        SABER_METRIC_COUNT(Diagnostics::Counter::OverlaysDropped);
        ESP_LOGW(TAG, "Clash overlay dropped: no free overlay slot");
    }

    ESP_LOGD(TAG, "Clash triggered: %s (G drop threshold: %.2f)", path.c_str(),
             m_def.clashThresholdG);
}

} // namespace InertialSaber::Effects
