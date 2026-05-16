#pragma once

#include "InertialDefinition.hpp"
#include "InertialEffect.hpp"
#include "AudioEngine.hpp"
#include "inertial_swing/AudioPathProvider.hpp"
#include "inertial_swing/SwingSwapper.hpp"

#include <atomic>
#include <cstdint>

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
                        const Core::InertialDefinition& definition);

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
    const Core::InertialDefinition& m_def;
    InertialSwing::AudioPathProvider m_audioProvider;
    InertialSwing::SwingSwapper m_swapper;
    
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

    float computeMasterVolume() const;
    float computeFinalMix() const;
    void applySwingVolumes(float masterVolume, float finalMix);
    void applyHumDucking(float masterVolume);
    void handleInertialBurst();
    void executeSwap();
};

} // namespace InertialSaber::Effects
