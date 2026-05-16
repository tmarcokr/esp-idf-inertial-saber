#include "inertial_swing/InertialSwingEffect.hpp"

#include "esp_log.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace InertialSaber::Effects {

using Espressif::Wrappers::Audio::INVALID_CHANNEL;

static InertialSwing::SwingFontConfig buildFontConfig(const Core::InertialDefinition& def) {
    return {
        .basePath       = std::string("/sdcard/") + def.profileRoot,
        .humCount       = def.fontHumCount,
        .swingPairCount = def.fontSwingPairCount,
        .burstCount     = def.fontBurstCount,
    };
}

InertialSwingEffect::InertialSwingEffect(
    Espressif::Wrappers::Audio::AudioEngine& engine,
    const Core::InertialDefinition& definition)
    : m_engine(engine)
    , m_def(definition)
    , m_audioProvider(buildFontConfig(definition)) {
    Priority = 0;
}

void InertialSwingEffect::activate() {
    if (m_active.load()) return;

    m_chHum = m_engine.play(m_audioProvider.provideHumPath(), true, m_def.humBaseVolume);
    
    auto paths = m_audioProvider.provideSwingPaths();
    m_chSwingL = m_engine.play(paths.low, true, 0);
    m_chSwingH = m_engine.play(paths.high, true, 0);

    m_swapper.reset();

    m_active.store(true);
    ESP_LOGI(TAG, "Activated — pair %u, hum=%d, swL=%d, swH=%d",
             m_audioProvider.getCurrentPairIndex(), m_chHum, m_chSwingL, m_chSwingH);
}

void InertialSwingEffect::deactivate() {
    if (!m_active.load()) return;

    m_active.store(false);

    if (m_chHum != INVALID_CHANNEL) {
        m_engine.stop(m_chHum);
        m_chHum = INVALID_CHANNEL;
    }
    if (m_chSwingL != INVALID_CHANNEL) {
        m_engine.stop(m_chSwingL);
        m_chSwingL = INVALID_CHANNEL;
    }
    if (m_chSwingH != INVALID_CHANNEL) {
        m_engine.stop(m_chSwingH);
        m_chSwingH = INVALID_CHANNEL;
    }

    ESP_LOGI(TAG, "Deactivated — all channels stopped");
}

bool InertialSwingEffect::isActive() const {
    return m_active.load();
}

bool InertialSwingEffect::Test(const Core::SaberDataPacket& packet) {
    m_kineticEnergy = packet.KineticEnergy;
    m_orientationVector = packet.OrientationVector;
    m_inertialOverload = packet.InertialOverload;
    m_inertialBurst = packet.InertialBurst;
    m_timestampMs = packet.timestamp_ms;

    return m_active.load();
}

void InertialSwingEffect::Run() {
    if (m_chHum == INVALID_CHANNEL || m_chSwingL == INVALID_CHANNEL || m_chSwingH == INVALID_CHANNEL) return;

    float masterVolume = computeMasterVolume();
    float finalMix = computeFinalMix();

    applySwingVolumes(masterVolume, finalMix);
    applyHumDucking(masterVolume);
    handleInertialBurst();
    
    if (m_swapper.evaluateSwap(masterVolume, m_timestampMs,
                               m_def.swingSwapMinVolume, m_def.swingSwapCooldownMs)) {
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

    float gravityMod = m_orientationVector * m_def.gravityInfluence;
    return std::clamp(baseMix + gravityMod, 0.0f, 1.0f);
}

void InertialSwingEffect::applySwingVolumes(float masterVolume, float finalMix) {
    auto volL = static_cast<uint16_t>(masterVolume * (1.0f - finalMix) * kMaxVolume14bit);
    auto volH = static_cast<uint16_t>(masterVolume * finalMix * kMaxVolume14bit);

    m_engine.setChannelVolume(m_chSwingL, volL);
    m_engine.setChannelVolume(m_chSwingH, volH);

    if (++m_logCounter >= 400) { // ~500ms at 800Hz
        m_logCounter = 0;
        auto humVol = static_cast<uint16_t>(m_def.humBaseVolume * std::max(0.0f, 1.0f - masterVolume * m_def.humMaxDucking));
        ESP_LOGI(TAG, "KE:%.2f | MV:%.2f | Mix:%.2f | L:%u H:%u | Hum:%u | OL:%.2f | Pair:%u",
                 m_kineticEnergy, masterVolume, finalMix,
                 volL, volH, humVol, m_inertialOverload, m_audioProvider.getCurrentPairIndex());
    }
}

void InertialSwingEffect::applyHumDucking(float masterVolume) {
    float duckingAmount = masterVolume * m_def.humMaxDucking;
    float humRatio = std::max(0.0f, 1.0f - duckingAmount);
    auto humVol = static_cast<uint16_t>(m_def.humBaseVolume * humRatio);

    m_engine.setChannelVolume(m_chHum, humVol);
}

void InertialSwingEffect::handleInertialBurst() {
    if (!m_inertialBurst || m_audioProvider.getConfig().burstCount == 0) return;

    m_engine.play(m_audioProvider.provideBurstPath(), false, kMaxVolume14bit);

    ESP_LOGI(TAG, "Inertial Burst triggered");
}

void InertialSwingEffect::executeSwap() {
    if (m_audioProvider.getConfig().swingPairCount <= 1) return;

    if (m_chSwingL != INVALID_CHANNEL) m_engine.stop(m_chSwingL);
    if (m_chSwingH != INVALID_CHANNEL) m_engine.stop(m_chSwingH);

    auto paths = m_audioProvider.provideSwingPaths();

    m_chSwingL = m_engine.play(paths.low, true, 0);
    m_chSwingH = m_engine.play(paths.high, true, 0);

    ESP_LOGI(TAG, "Pair swapped → %u (swL=%d, swH=%d)",
             m_audioProvider.getCurrentPairIndex(), m_chSwingL, m_chSwingH);
}

} // namespace InertialSaber::Effects
