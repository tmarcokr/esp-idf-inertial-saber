#pragma once

#include "InertialEffect.hpp"
#include "AudioEngine.hpp"

#include <atomic>
#include <cstdint>
#include <string>

namespace InertialSaber::Effects {

/**
 * @brief Configuration for sound font file paths and counts.
 *
 * Encapsulates the SD card directory structure and file inventory
 * for a single sound font profile. File counts determine the range
 * of random selection for each audio category.
 */
struct SwingFontConfig {
    std::string basePath;
    uint8_t humCount;
    uint8_t swingPairCount;
    uint8_t burstCount;
};

/**
 * @brief Physics-driven audio engine implementing the InertialSwing specification.
 *
 * Priority 0 Flow Modulator. Transforms kinetic data from the SaberDataPacket
 * into real-time volume commands on three persistent audio channels (hum, swingL,
 * swingH) plus one-shot triggers for Inertial Burst events.
 *
 * Subsystems:
 *   - Inertial Crossfade: maps KineticEnergy to MasterVolume and L/H balance
 *   - Gravity Tonal Modulator: biases L/H balance by blade orientation
 *   - Zero-Volume SD Swapper: cycles swing pairs at calm moments
 *   - Inertial Burst: fires random swng one-shots on InertialBurst events
 *
 * Thread safety:
 *   - activate() / deactivate(): safe from any task (AudioEngine is mutex-protected)
 *   - Test() / Run(): called exclusively from the bus task
 */
class InertialSwingEffect final : public Core::InertialEffect {
public:
    InertialSwingEffect(Espressif::Wrappers::Audio::AudioEngine& engine,
                        const SwingFontConfig& fontConfig);

    /**
     * @brief Start audio playback: hum loop + initial random swing pair at volume 0.
     */
    void activate();

    /**
     * @brief Stop all audio channels and reset internal state.
     */
    void deactivate();

    [[nodiscard]] bool isActive() const;

    bool Test(const Core::SaberDataPacket& packet) override;
    void Run() override;

private:
    static constexpr const char* TAG = "InertialSwing";
    static constexpr uint16_t kMaxVolume14bit = 16384;

    Espressif::Wrappers::Audio::AudioEngine& m_engine;
    SwingFontConfig m_fontConfig;
    std::atomic<bool> m_active{false};

    Espressif::Wrappers::Audio::ChannelId m_chHum    = Espressif::Wrappers::Audio::INVALID_CHANNEL;
    Espressif::Wrappers::Audio::ChannelId m_chSwingL = Espressif::Wrappers::Audio::INVALID_CHANNEL;
    Espressif::Wrappers::Audio::ChannelId m_chSwingH = Espressif::Wrappers::Audio::INVALID_CHANNEL;

    uint8_t m_currentPairIndex = 0;
    bool m_needsSwap = false;
    bool m_wasMoving = false;
    uint32_t m_lastMovementTimeMs = 0;

    float m_kineticEnergy = 0.0f;
    float m_orientationVector = 0.0f;
    float m_inertialOverload = 0.0f;
    bool m_inertialBurst = false;
    uint32_t m_timestampMs = 0;

    uint32_t m_logCounter = 0;

    float computeMasterVolume() const;
    float computeFinalMix() const;
    void applySwingVolumes(float masterVolume, float finalMix);
    void applyHumDucking(float masterVolume);
    void handleInertialBurst();
    void handleSwapperStateMachine(float masterVolume);
    void executeSwap();
    [[nodiscard]] uint8_t randomInRange(uint8_t count) const;
    [[nodiscard]] std::string buildHumPath() const;
    [[nodiscard]] std::string buildSwingLPath(uint8_t pairIndex) const;
    [[nodiscard]] std::string buildSwingHPath(uint8_t pairIndex) const;
    [[nodiscard]] std::string buildBurstPath(uint8_t fileIndex) const;
};

} // namespace InertialSaber::Effects
