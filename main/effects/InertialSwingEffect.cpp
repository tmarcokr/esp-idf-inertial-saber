#include "InertialSwingEffect.hpp"
#include "PlatformConfig.hpp"

#include "esp_log.h"
#include "esp_random.h"

#include <algorithm>
#include <cmath>

namespace InertialSaber::Effects {

using namespace Core::Platform;
using Espressif::Wrappers::Audio::INVALID_CHANNEL;

InertialSwingEffect::InertialSwingEffect(
    Espressif::Wrappers::Audio::AudioEngine& engine,
    const SwingFontConfig& fontConfig)
    : m_engine(engine)
    , m_fontConfig(fontConfig) {
    Priority = 0;
}

void InertialSwingEffect::activate() {
    if (m_active.load()) return;

    m_currentPairIndex = randomInRange(m_fontConfig.swingPairCount);

    m_chHum = m_engine.play(buildHumPath(), true, kHumBaseVolume);
    m_chSwingL = m_engine.play(buildSwingLPath(m_currentPairIndex), true, 0);
    m_chSwingH = m_engine.play(buildSwingHPath(m_currentPairIndex), true, 0);

    m_needsSwap = false;
    m_wasMoving = false;

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
    handleSwapperStateMachine(masterVolume);
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
                 volL, volH, humVol, m_inertialOverload, m_currentPairIndex);
    }
}

void InertialSwingEffect::applyHumDucking(float masterVolume) {

    float duckingAmount = masterVolume * kHumMaxDucking;
    float humRatio = std::max(0.0f, 1.0f - duckingAmount);
    auto humVol = static_cast<uint16_t>(kHumBaseVolume * humRatio);

    m_engine.setChannelVolume(m_chHum, humVol);
}

void InertialSwingEffect::handleInertialBurst() {
    if (!m_inertialBurst || m_fontConfig.burstCount == 0) return;

    uint8_t fileIndex = randomInRange(m_fontConfig.burstCount);
    m_engine.play(buildBurstPath(fileIndex), false, kMaxVolume14bit);

    ESP_LOGI(TAG, "Inertial Burst — playing swng%u", fileIndex + 1);
}

void InertialSwingEffect::handleSwapperStateMachine(float masterVolume) {
    if (masterVolume > Core::Platform::kSwingSwapMinVolume) {
        m_needsSwap = true;
    }

    bool isMoving = masterVolume > 0.0f;

    if (isMoving) {
        m_wasMoving = true;
        m_lastMovementTimeMs = m_timestampMs;
    } else {
        if (m_wasMoving) {
            // Just stopped moving
            m_wasMoving = false;
        } else if (m_needsSwap) {
            // Have been stopped for a while, check cooldown
            uint32_t idleTime = m_timestampMs - m_lastMovementTimeMs;
            if (idleTime >= Core::Platform::kSwingSwapCooldownMs) {
                executeSwap();
                m_needsSwap = false;
            }
        }
    }
}

void InertialSwingEffect::executeSwap() {
    if (m_fontConfig.swingPairCount <= 1) return;

    if (m_chSwingL != INVALID_CHANNEL) m_engine.stop(m_chSwingL);
    if (m_chSwingH != INVALID_CHANNEL) m_engine.stop(m_chSwingH);

    uint8_t newPair = randomInRange(m_fontConfig.swingPairCount);
    // Avoid repeating the same pair
    while (newPair == m_currentPairIndex && m_fontConfig.swingPairCount > 1) {
        newPair = randomInRange(m_fontConfig.swingPairCount);
    }
    m_currentPairIndex = newPair;

    m_chSwingL = m_engine.play(buildSwingLPath(m_currentPairIndex), true, 0);
    m_chSwingH = m_engine.play(buildSwingHPath(m_currentPairIndex), true, 0);

    ESP_LOGI(TAG, "Pair swapped → %u (swL=%d, swH=%d)",
             m_currentPairIndex, m_chSwingL, m_chSwingH);
}



uint8_t InertialSwingEffect::randomInRange(uint8_t count) const {
    if (count == 0) return 0;
    return static_cast<uint8_t>(esp_random() % count);
}

std::string InertialSwingEffect::buildHumPath() const {
    return m_fontConfig.basePath + "/hum.wav";
}

std::string InertialSwingEffect::buildSwingLPath(uint8_t pairIndex) const {
    return m_fontConfig.basePath + "/swingl/swingl" + std::to_string(pairIndex + 1) + ".wav";
}

std::string InertialSwingEffect::buildSwingHPath(uint8_t pairIndex) const {
    return m_fontConfig.basePath + "/swingh/swingh" + std::to_string(pairIndex + 1) + ".wav";
}

std::string InertialSwingEffect::buildBurstPath(uint8_t fileIndex) const {
    return m_fontConfig.basePath + "/swng/swng" + std::to_string(fileIndex + 1) + ".wav";
}

} // namespace InertialSaber::Effects
