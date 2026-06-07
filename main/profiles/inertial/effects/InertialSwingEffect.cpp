#include "InertialSwingEffect.hpp"

#include "esp_log.h"
#include "esp_random.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace InertialSaber::Effects {

using Espressif::Wrappers::Audio::INVALID_CHANNEL;

InertialSwingEffect::InertialSwingEffect(
    Espressif::Wrappers::Audio::AudioEngine& engine,
    const InertialSaber::Profiles::Inertial::InertialDefinition& definition)
    : m_engine(engine)
    , m_def(definition)
    , m_humPath(std::string("/sdcard/") + definition.profileRoot + "/hum.wav") {
    Priority = 0;
}

void InertialSwingEffect::activate() {
    if (m_active.load()) return;

    m_chHum = m_engine.play(m_humPath, true, m_def.humBaseVolume);
    
    auto paths = provideSwingPaths();
    m_chSwingL = m_engine.play(paths.low, true, 0);
    m_chSwingH = m_engine.play(paths.high, true, 0);

    m_needsSwap = false;
    m_wasMoving = false;
    m_lastMovementTimeMs = 0;

    m_active.store(true);
    ESP_LOGI(TAG, "Activated — pair %u, hum=%d, swL=%d, swH=%d",
             m_currentPairIndex, m_chHum, m_chSwingL, m_chSwingH);
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

    float gravityMod = m_orientationVector * m_def.gravityInfluence;
    return std::clamp(baseMix + gravityMod, 0.0f, 1.0f);
}

void InertialSwingEffect::applySwingVolumes(float masterVolume, float finalMix) {
    auto volL = static_cast<uint16_t>(masterVolume * (1.0f - finalMix) * kMaxVolume14bit);
    auto volH = static_cast<uint16_t>(masterVolume * finalMix * kMaxVolume14bit);

    m_engine.setChannelVolume(m_chSwingL, volL);
    m_engine.setChannelVolume(m_chSwingH, volH);

    if (++m_logCounter >= 400) { 
        m_logCounter = 0;
        auto humVol = static_cast<uint16_t>(m_def.humBaseVolume * std::max(0.0f, 1.0f - masterVolume * m_def.humMaxDucking));
        ESP_LOGI(TAG, "KE:%.2f | MV:%.2f | Mix:%.2f | L:%u H:%u | Hum:%u | OL:%.2f | Pair:%u",
                 m_kineticEnergy, masterVolume, finalMix,
                 volL, volH, humVol, m_inertialOverload, m_currentPairIndex);
    }
}

void InertialSwingEffect::applyHumDucking(float masterVolume) {
    float duckingAmount = masterVolume * m_def.humMaxDucking;
    float humRatio = std::max(0.0f, 1.0f - duckingAmount);
    auto humVol = static_cast<uint16_t>(m_def.humBaseVolume * humRatio);

    m_engine.setChannelVolume(m_chHum, humVol);
}

void InertialSwingEffect::handleInertialBurst() {
    if (!m_inertialBurst || m_def.fontBurstCount == 0) return;

    m_engine.play(provideBurstPath(), false, kMaxVolume14bit);

    ESP_LOGI(TAG, "Inertial Burst triggered");
}

InertialSwingEffect::SwingPathPair InertialSwingEffect::provideSwingPaths() {
    if (m_def.fontSwingPairCount > 1) {
        uint8_t newPair;
        do {
            newPair = static_cast<uint8_t>(esp_random() % m_def.fontSwingPairCount);
        } while (newPair == m_currentPairIndex);
        m_currentPairIndex = newPair;
    } else {
        m_currentPairIndex = 0;
    }
    
    std::string prefix = std::string("/sdcard/") + m_def.profileRoot + "/swing";
    std::string suffix = std::to_string(m_currentPairIndex + 1) + ".wav";
    
    return { prefix + "l/swingl" + suffix, prefix + "h/swingh" + suffix };
}

std::string InertialSwingEffect::provideBurstPath() const {
    uint8_t idx = static_cast<uint8_t>(esp_random() % m_def.fontBurstCount) + 1;
    return std::string("/sdcard/") + m_def.profileRoot + "/swng/swng" + std::to_string(idx) + ".wav";
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
    if (m_def.fontSwingPairCount <= 1) return;

    if (m_chSwingL != INVALID_CHANNEL) m_engine.stop(m_chSwingL);
    if (m_chSwingH != INVALID_CHANNEL) m_engine.stop(m_chSwingH);

    auto paths = provideSwingPaths();

    m_chSwingL = m_engine.play(paths.low, true, 0);
    m_chSwingH = m_engine.play(paths.high, true, 0);

    ESP_LOGI(TAG, "Pair swapped → %u (swL=%d, swH=%d)",
             m_currentPairIndex, m_chSwingL, m_chSwingH);
}

} // namespace InertialSaber::Effects
