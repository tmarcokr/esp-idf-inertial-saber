#pragma once

#include "core/InertialEffect.hpp"
#include <string>

namespace InertialSaber::Core {
struct SaberDataPacket;
}

namespace InertialSaber::Profiles {
class ConfigurableProfile;
}

namespace Espressif::Wrappers { class RgbLed; }

namespace Espressif::Wrappers::Audio {
class AudioEngine;
}

#if CONFIG_IDF_TARGET_ESP32S3
namespace InertialSaber::System {
class PsramAudioCache;
}
#endif

namespace InertialSaber::Effects {

/**
 * @brief Effect that blocks input and waits for the PSRAM preload to complete before unlocking the saber and playing the selection sound.
 */
class PreloadWaitEffect final : public Core::InertialEffect {
public:
    PreloadWaitEffect(
        Profiles::ConfigurableProfile&           profile,
        Espressif::Wrappers::Audio::AudioEngine& audio,
        Espressif::Wrappers::RgbLed*             statusLed
#if CONFIG_IDF_TARGET_ESP32S3
        , System::PsramAudioCache*               psramCache
#endif
    );

    bool Test(const Core::SaberDataPacket& packet) override;
    void Run() override;

private:
    Profiles::ConfigurableProfile&           m_profile;
    Espressif::Wrappers::Audio::AudioEngine& m_audio;
    Espressif::Wrappers::RgbLed*             m_statusLed;
#if CONFIG_IDF_TARGET_ESP32S3
    System::PsramAudioCache*                 m_psramCache;
#endif
};

} // namespace InertialSaber::Effects
