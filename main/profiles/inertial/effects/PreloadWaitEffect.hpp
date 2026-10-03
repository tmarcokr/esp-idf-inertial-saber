#pragma once

#include "core/InertialEffect.hpp"

#include <cstdint>
#include <optional>

namespace InertialSaber::Core {
struct SaberDataPacket;
}

namespace InertialSaber::Profiles {
class PowerStateMachine;
class SoundFont;
} // namespace InertialSaber::Profiles

namespace InertialSaber::System {
class AudioController;
class PsramAudioCache;
} // namespace InertialSaber::System

namespace InertialSaber::System::Status {
class StatusIndicator;
}

namespace InertialSaber::Effects {

/** @brief Effect that blocks input, waits for the PSRAM preload, then unlocks the saber and plays the selection sound, or faults it on failure. */
class PreloadWaitEffect final : public Core::InertialEffect {
public:
    /**
     * @param switchRequestedUs Time of the profile switch request that built this effect
     *        (esp_timer, µs, truncated to 32 bits); empty at boot.
     */
    PreloadWaitEffect(Profiles::PowerStateMachine& power, System::AudioController& audio,
                      const System::PsramAudioCache& audioCache,
                      System::Status::StatusIndicator& status, const Profiles::SoundFont& font,
                      std::optional<uint32_t> switchRequestedUs);

    bool test(const Core::SaberDataPacket& packet) override;
    void run() override;

private:
    Profiles::PowerStateMachine& m_power;
    System::AudioController& m_audio;
    const System::PsramAudioCache& m_audioCache;
    System::Status::StatusIndicator& m_status;
    const Profiles::SoundFont& m_font;
    const std::optional<uint32_t> m_switchRequestedUs;
};

} // namespace InertialSaber::Effects
