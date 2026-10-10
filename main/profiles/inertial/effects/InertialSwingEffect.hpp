// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "profiles/inertial/InertialDefinition.hpp"
#include "core/InertialEffect.hpp"
#include "system/audio/AudioController.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace InertialSaber::Profiles {
class SoundFont;
}

namespace InertialSaber::Effects {

/**
 * @brief Physics-driven audio engine implementing the InertialSwing specification.
 *
 * Priority 0 Flow Modulator. Transforms kinetic data from the SaberDataPacket
 * into real-time volume commands on three persistent audio voices (hum, swingL,
 * swingH) plus one-shot triggers for Inertial Burst events.
 */
class InertialSwingEffect final : public Core::InertialEffect {
public:
    /** @brief Audio voices acquired per instance (hum, swingL, swingH). */
    static constexpr size_t kVoiceCount = 3;

    InertialSwingEffect(InertialSaber::System::AudioController& audio,
                        const InertialSaber::Profiles::Inertial::InertialDefinition& definition,
                        const InertialSaber::Profiles::SoundFont& font);

    /**
     * @brief Start audio playback: hum loop + initial random swing pair, linked and at volume 0
     * (no swing voices when the font has no swing pairs).
     */
    void activate();

    /**
     * @brief Stop all audio voices and reset internal state.
     */
    void deactivate();

    bool test(const Core::SaberDataPacket& packet) override;
    void run() override;

private:
    static constexpr const char* TAG = "InertialSwing";
    static constexpr uint32_t kTelemetryLogIntervalMs = 2000;

    InertialSaber::System::AudioController& m_audio;
    const InertialSaber::Profiles::Inertial::InertialDefinition& m_def;
    const InertialSaber::Profiles::SoundFont& m_font;

    std::atomic<bool> m_active{false};

    InertialSaber::System::AudioVoice m_hum;
    InertialSaber::System::AudioVoice m_swingLow;
    InertialSaber::System::AudioVoice m_swingHigh;

    float m_kineticEnergy = 0.0f;
    float m_orientation = 0.0f;
    float m_inertialOverload = 0.0f;
    bool m_inertialBurst = false;
    uint32_t m_timestampMs = 0;

    uint32_t m_lastTelemetryMs = 0;

    uint8_t m_currentPairIndex = 0;

    // Swing Swapper state
    bool m_needsSwap = false;
    bool m_wasMoving = false;
    uint32_t m_lastMovementTimeMs = 0;

    struct SwingPathPair {
        InertialSaber::System::AudioPath low;
        InertialSaber::System::AudioPath high;
    };

    float computeMasterVolume() const;
    float computeFinalMix() const;
    void applySwingVolumes(float masterVolume, float finalMix);
    void applyHumDucking(float masterVolume);
    void handleInertialBurst();

    SwingPathPair provideSwingPaths();
    bool playSwingPair();
    bool evaluateSwap(float masterVolume);
    void executeSwap();
};

} // namespace InertialSaber::Effects
