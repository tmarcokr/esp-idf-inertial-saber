#include "InertialSwingEffect.hpp"
#include "diagnostics/Metrics.hpp"
#include "AudioLevels.hpp"
#include "profiles/SoundFont.hpp"
#include "system/PsramAudioCache.hpp"

#include "esp_log.h"
#include "esp_random.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace InertialSaber::Effects {

InertialSwingEffect::InertialSwingEffect(
    InertialSaber::System::AudioController& audio,
    const InertialSaber::Profiles::Inertial::InertialDefinition& definition,
    const InertialSaber::Profiles::SoundFont& font,
    const InertialSaber::System::PsramAudioCache& audioCache)
    : InertialEffect(0)
    , m_audio(audio)
    , m_def(definition)
    , m_font(font)
    , m_audioCache(audioCache)
    , m_hum(audio.acquireVoice())
    , m_swingLow(audio.acquireVoice())
    , m_swingHigh(audio.acquireVoice()) {
    if (!m_hum.valid() || !m_swingLow.valid() || !m_swingHigh.valid()) {
        ESP_LOGE(TAG, "Missing audio voice (hum=%d swingL=%d swingH=%d); those layers stay silent",
                 m_hum.valid(), m_swingLow.valid(), m_swingHigh.valid());
    }
}

void InertialSwingEffect::activate() {
    if (m_active.load()) return;
    SABER_METRIC_SCOPE(Diagnostics::Metric::SwingActivate);

    m_hum.play(InertialSaber::System::PsramAudioCache::humPath(), true, m_def.humBaseVolume);

    const SwingPathPair paths = provideSwingPaths();
    m_swingLow.play(paths.low, true, 0);
    m_swingHigh.play(paths.high, true, 0);

    m_needsSwap = false;
    m_wasMoving = false;
    m_lastMovementTimeMs = 0;

    m_active.store(true);
    ESP_LOGD(TAG, "Activated — pair %u", m_currentPairIndex);
}

void InertialSwingEffect::deactivate() {
    if (!m_active.load()) return;

    m_active.store(false);

    m_hum.stop();
    m_swingLow.stop();
    m_swingHigh.stop();

    ESP_LOGD(TAG, "Deactivated — all voices stopped");
}

bool InertialSwingEffect::test(const Core::SaberDataPacket& packet) {
    m_kineticEnergy = packet.kineticEnergy;
    m_orientation = packet.orientation;
    m_inertialOverload = packet.inertialOverload;
    m_inertialBurst = packet.inertialBurst;
    m_timestampMs = packet.timestampMs;

    return m_active.load();
}

void InertialSwingEffect::run() {
    SABER_METRIC_SCOPE(Diagnostics::Metric::RunSwing);
    float masterVolume = computeMasterVolume();
    float finalMix = computeFinalMix();

    applySwingVolumes(masterVolume, finalMix);
    applyHumDucking(masterVolume);
    handleInertialBurst();

    if (evaluateSwap(masterVolume)) {
        executeSwap();
    }
}

float InertialSwingEffect::computeMasterVolume() const {
    float range = m_def.swingMaxThresholdG - m_def.swingIdleThresholdG;
    float normalized = (m_kineticEnergy - m_def.swingIdleThresholdG) / range;
    return std::clamp(normalized, 0.0f, 1.0f);
}

float InertialSwingEffect::computeFinalMix() const {
    float crossfadeRange = m_def.swingCrossfadeHighG - m_def.swingCrossfadeLowG;
    float baseMix = (m_kineticEnergy - m_def.swingCrossfadeLowG) / crossfadeRange;
    baseMix = std::clamp(baseMix, 0.0f, 1.0f);

    float bladeAngleRad = m_orientation * (std::numbers::pi_v<float> / 2.0f);
    float gravityMod = std::sin(bladeAngleRad) * m_def.gravityInfluence;
    return std::clamp(baseMix + gravityMod, 0.0f, 1.0f);
}

void InertialSwingEffect::applySwingVolumes(float masterVolume, float finalMix) {
    auto volL = static_cast<uint16_t>(masterVolume * (1.0f - finalMix) * kFullVolume);
    auto volH = static_cast<uint16_t>(masterVolume * finalMix * kFullVolume);

    m_swingLow.setVolume(volL);
    m_swingHigh.setVolume(volH);

    if (++m_logCounter >= kTelemetryLogIntervalCycles) {
        m_logCounter = 0;
        auto humVol = static_cast<uint16_t>(
            m_def.humBaseVolume * std::max(0.0f, 1.0f - masterVolume * m_def.humMaxDucking));
        ESP_LOGD(TAG, "KE:%.2f | MV:%.2f | Mix:%.2f | L:%u H:%u | Hum:%u | OL:%.2f | Pair:%u",
                 m_kineticEnergy, masterVolume, finalMix, volL, volH, humVol, m_inertialOverload,
                 m_currentPairIndex);
    }
}

void InertialSwingEffect::applyHumDucking(float masterVolume) {
    float duckingAmount = masterVolume * m_def.humMaxDucking;
    float humRatio = std::max(0.0f, 1.0f - duckingAmount);
    auto humVol = static_cast<uint16_t>(m_def.humBaseVolume * humRatio);

    m_hum.setVolume(humVol);
}

void InertialSwingEffect::handleInertialBurst() {
    if (!m_inertialBurst || m_font.count(Profiles::FontCategory::Burst) == 0) return;

    m_audio.playOneShot(m_font.randomPath(Profiles::FontCategory::Burst), kFullVolume);

    ESP_LOGD(TAG, "Inertial Burst triggered");
}

InertialSwingEffect::SwingPathPair InertialSwingEffect::provideSwingPaths() {
    uint8_t availablePairs = m_audioCache.loadedSwingPairCount();
    if (availablePairs > 1) {
        uint8_t newPair;
        do {
            newPair = static_cast<uint8_t>(esp_random() % availablePairs);
        } while (newPair == m_currentPairIndex);
        m_currentPairIndex = newPair;
    } else {
        m_currentPairIndex = 0;
    }
    const auto pairNumber = static_cast<uint8_t>(m_currentPairIndex + 1);
    return {InertialSaber::System::PsramAudioCache::swingLowPath(pairNumber),
            InertialSaber::System::PsramAudioCache::swingHighPath(pairNumber)};
}

bool InertialSwingEffect::evaluateSwap(float masterVolume) {
    if (masterVolume > m_def.swingSwapMinVolume) {
        m_needsSwap = true;
    }

    bool isMoving = masterVolume > 0.0f;

    if (isMoving) {
        m_wasMoving = true;
        m_lastMovementTimeMs = m_timestampMs;
    } else {
        if (m_wasMoving) {
            m_wasMoving = false;
        } else if (m_needsSwap) {
            uint32_t idleTime = m_timestampMs - m_lastMovementTimeMs;
            if (idleTime >= m_def.swingSwapCooldownMs) {
                m_needsSwap = false;
                return true;
            }
        }
    }

    return false;
}

void InertialSwingEffect::executeSwap() {
    uint8_t availablePairs = m_audioCache.loadedSwingPairCount();
    if (availablePairs <= 1) return;
    SABER_METRIC_SCOPE(Diagnostics::Metric::SwingSwap);

    m_swingLow.stop();
    m_swingHigh.stop();

    const SwingPathPair paths = provideSwingPaths();

    m_swingLow.play(paths.low, true, 0);
    m_swingHigh.play(paths.high, true, 0);

    ESP_LOGD(TAG, "Pair swapped → %u", m_currentPairIndex);
}

} // namespace InertialSaber::Effects
