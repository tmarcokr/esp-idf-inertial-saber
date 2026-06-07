#pragma once

#include "profiles/inertial/InertialDefinition.hpp"
#include "core/InertialEffect.hpp"
#include "AudioEngine.hpp"

#include <atomic>
#include <cstdint>
#include <string>

namespace InertialSaber::Effects {

/**
 * @brief Physics-driven audio engine implementing the InertialSwing specification.
 *
 * Priority 0 Flow Modulator. Transforms kinetic data from the SaberDataPacket
 * into real-time volume commands on three persistent audio channels (hum, swingL,
 * swingH) plus one-shot triggers for Inertial Burst events.
 */
class InertialSwingEffect final : public Core::InertialEffect {
public:
    InertialSwingEffect(Espressif::Wrappers::Audio::AudioEngine& engine,
                        const InertialSaber::Profiles::Inertial::InertialDefinition& definition);

    /**
     * @brief Start audio playback: hum loop + initial random swing pair at volume 0.
     */
    void activate();

    /**
     * @brief Stop all audio channels and reset internal state.
     */
    void deactivate();

    /**
     * @brief Checks if the effect is currently active.
     * @return True if active.
     */
    [[nodiscard]] bool isActive() const;

    bool Test(const Core::SaberDataPacket& packet) override;
    void Run() override;

private:
    static constexpr const char* TAG = "InertialSwing";
    static constexpr uint16_t kMaxVolume14bit = 16384;

    Espressif::Wrappers::Audio::AudioEngine& m_engine;
    const InertialSaber::Profiles::Inertial::InertialDefinition& m_def;
    
    std::atomic<bool> m_active{false};

    Espressif::Wrappers::Audio::ChannelId m_chHum    = Espressif::Wrappers::Audio::INVALID_CHANNEL;
    Espressif::Wrappers::Audio::ChannelId m_chSwingL = Espressif::Wrappers::Audio::INVALID_CHANNEL;
    Espressif::Wrappers::Audio::ChannelId m_chSwingH = Espressif::Wrappers::Audio::INVALID_CHANNEL;

    float m_kineticEnergy = 0.0f;
    float m_orientationVector = 0.0f;
    float m_inertialOverload = 0.0f;
    bool m_inertialBurst = false;
    uint32_t m_timestampMs = 0;

    uint32_t m_logCounter = 0;

    // Audio Provider state
    std::string m_humPath;
    uint8_t m_currentPairIndex = 0;

    // Swing Swapper state
    bool m_needsSwap = false;
    bool m_wasMoving = false;
    uint32_t m_lastMovementTimeMs = 0;

    struct SwingPathPair {
        std::string low;
        std::string high;
    };

    float computeMasterVolume() const;
    float computeFinalMix() const;
    void applySwingVolumes(float masterVolume, float finalMix);
    void applyHumDucking(float masterVolume);
    void handleInertialBurst();
    
    SwingPathPair provideSwingPaths();
    std::string provideBurstPath() const;
    bool evaluateSwap(float masterVolume);
    void executeSwap();
};

} // namespace InertialSaber::Effects
