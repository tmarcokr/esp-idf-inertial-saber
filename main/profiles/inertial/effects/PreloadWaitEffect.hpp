#pragma once

#include "core/InertialEffect.hpp"

namespace InertialSaber::Core {
struct SaberDataPacket;
}

namespace InertialSaber::Profiles {
class PowerStateMachine;
class SoundFont;
}

namespace Espressif::Wrappers::Audio {
class AudioEngine;
}

namespace InertialSaber::System {
class PsramAudioCache;
}

namespace InertialSaber::System::Status {
class StatusIndicator;
}

namespace InertialSaber::Effects {

/**
 * @brief Effect that blocks input and waits for the PSRAM preload to complete before unlocking the saber and playing the selection sound.
 */
class PreloadWaitEffect final : public Core::InertialEffect {
public:
    PreloadWaitEffect(Profiles::PowerStateMachine& power,
                      Espressif::Wrappers::Audio::AudioEngine& audio,
                      const System::PsramAudioCache& audioCache,
                      System::Status::StatusIndicator& status,
                      const Profiles::SoundFont& font);

    bool test(const Core::SaberDataPacket& packet) override;
    void run() override;

private:
    Profiles::PowerStateMachine&             m_power;
    Espressif::Wrappers::Audio::AudioEngine& m_audio;
    const System::PsramAudioCache&           m_audioCache;
    System::Status::StatusIndicator&         m_status;
    const Profiles::SoundFont&               m_font;
};

} // namespace InertialSaber::Effects
