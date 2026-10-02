#pragma once

#include "core/InertialEffect.hpp"

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
    PreloadWaitEffect(Profiles::PowerStateMachine& power, System::AudioController& audio,
                      const System::PsramAudioCache& audioCache,
                      System::Status::StatusIndicator& status, const Profiles::SoundFont& font);

    bool test(const Core::SaberDataPacket& packet) override;
    void run() override;

private:
    Profiles::PowerStateMachine& m_power;
    System::AudioController& m_audio;
    const System::PsramAudioCache& m_audioCache;
    System::Status::StatusIndicator& m_status;
    const Profiles::SoundFont& m_font;
};

} // namespace InertialSaber::Effects
