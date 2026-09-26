#pragma once

#include "core/InertialEffect.hpp"

namespace InertialSaber::Core {
struct SaberDataPacket;
}

namespace InertialSaber::Profiles {
class ConfigurableProfile;
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
    PreloadWaitEffect(Profiles::ConfigurableProfile& profile,
                      Espressif::Wrappers::Audio::AudioEngine& audio,
                      const System::PsramAudioCache& audioCache,
                      System::Status::StatusIndicator& status);

    bool test(const Core::SaberDataPacket& packet) override;
    void run() override;

private:
    Profiles::ConfigurableProfile&           m_profile;
    Espressif::Wrappers::Audio::AudioEngine& m_audio;
    const System::PsramAudioCache&           m_audioCache;
    System::Status::StatusIndicator&         m_status;
};

} // namespace InertialSaber::Effects
