#include "inertial_swing/InertialSwingEffect.hpp"
#include "PlatformConfig.hpp"

#include "esp_log.h"
#include <algorithm>
#include <cmath>

namespace InertialSaber::Effects {

using namespace Core::Platform;
using Espressif::Wrappers::Audio::INVALID_CHANNEL;

InertialSwingEffect::InertialSwingEffect(
    Espressif::Wrappers::Audio::AudioEngine& engine,
    const InertialSwing::SwingFontConfig& fontConfig)
    : m_engine(engine)
    , m_audioProvider(fontConfig) {
    Priority = 0;
}

void InertialSwingEffect::activate() {
    if (m_active.load()) return;

    m_chHum = m_engine.play(m_audioProvider.provideHumPath(), true, kHumBaseVolume);
    
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
    
    if (m_swapper.evaluateSwap(masterVolume, m_timestampMs)) {
        executeSwap();
    }
}

float InertialSwingEffect::computeMasterVolume() const {
    float range = kSwingMaxThresholdG - kSwingIdleThresholdG;
    float normalized = (m_kineticEnergy - kSwingIdleThresholdG) / range;
    return std::clamp(normalized, 0.0f, 1.0f);
}

float InertialSwingEffect::computeFinalMix() const {
    float crossfadeRange = kSwingCrossfadeHighG - kSwingCrossfadeLowG;
    float baseMix = (m_kineticEnergy - kSwingCrossfadeLowG) / crossfadeRange;
    baseMix = std::clamp(baseMix, 0.0f, 1.0f);

    float gravityMod = m_orientationVector * kGravityInfluence;
    return std::clamp(baseMix + gravityMod, 0.0f, 1.0f);
}

void InertialSwingEffect::applySwingVolumes(float masterVolume, float finalMix) {
    auto volL = static_cast<uint16_t>(masterVolume * (1.0f - finalMix) * kMaxVolume14bit);
    auto volH = static_cast<uint16_t>(masterVolume * finalMix * kMaxVolume14bit);

    m_engine.setChannelVolume(m_chSwingL, volL);
    m_engine.setChannelVolume(m_chSwingH, volH);

    if (++m_logCounter >= 400) { // ~500ms at 800Hz
        m_logCounter = 0;
        auto humVol = static_cast<uint16_t>(kHumBaseVolume * std::max(0.0f, 1.0f - masterVolume * kHumMaxDucking));
        ESP_LOGI(TAG, "KE:%.2f | MV:%.2f | Mix:%.2f | L:%u H:%u | Hum:%u | OL:%.2f | Pair:%u",
                 m_kineticEnergy, masterVolume, finalMix,
                 volL, volH, humVol, m_inertialOverload, m_audioProvider.getCurrentPairIndex());
    }
}

void InertialSwingEffect::applyHumDucking(float masterVolume) {
    float duckingAmount = masterVolume * kHumMaxDucking;
    float humRatio = std::max(0.0f, 1.0f - duckingAmount);
    auto humVol = static_cast<uint16_t>(kHumBaseVolume * humRatio);

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
