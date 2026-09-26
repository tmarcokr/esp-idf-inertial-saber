#include "PreloadWaitEffect.hpp"
#include "profiles/ConfigurableProfile.hpp"
#include "system/PsramAudioCache.hpp"
#include "system/status/StatusIndicator.hpp"
#include "AudioEngine.hpp"
#include "esp_log.h"

#include <string>

namespace InertialSaber::Effects {

static constexpr const char* TAG = "PreloadWait";

using System::Status::SystemStatus;

PreloadWaitEffect::PreloadWaitEffect(Profiles::ConfigurableProfile& profile,
                                     Espressif::Wrappers::Audio::AudioEngine& audio,
                                     const System::PsramAudioCache& audioCache,
                                     System::Status::StatusIndicator& status)
    : InertialEffect(0)
    , m_profile(profile)
    , m_audio(audio)
    , m_audioCache(audioCache)
    , m_status(status)
{}

bool PreloadWaitEffect::test(const Core::SaberDataPacket&) {
    return m_profile.getPowerState() == Profiles::ConfigurableProfile::PowerState::PRELOADING;
}

void PreloadWaitEffect::run() {
    if (!m_audioCache.isPreloadComplete()) {
        m_status.show(SystemStatus::Preloading);
        return;
    }

    m_profile.setPowerState(Profiles::ConfigurableProfile::PowerState::RETRACTED);

    m_status.show(SystemStatus::Ready);

    const auto& def = m_profile.definition();
    std::string fontPath = std::string("/sdcard/") + def.profileRoot + "font.wav";
    ESP_LOGI(TAG, "Preload complete. Playing selection sound: %s", fontPath.c_str());
    m_audio.play(fontPath, false, 16384);
}

} // namespace InertialSaber::Effects
