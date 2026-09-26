#include "PreloadWaitEffect.hpp"
#include "profiles/PowerStateMachine.hpp"
#include "profiles/SoundFont.hpp"
#include "AudioLevels.hpp"
#include "system/PsramAudioCache.hpp"
#include "system/status/StatusIndicator.hpp"
#include "AudioEngine.hpp"
#include "esp_log.h"

#include <string>

namespace InertialSaber::Effects {

static constexpr const char* TAG = "PreloadWait";

using System::Status::SystemStatus;

PreloadWaitEffect::PreloadWaitEffect(Profiles::PowerStateMachine& power,
                                     Espressif::Wrappers::Audio::AudioEngine& audio,
                                     const System::PsramAudioCache& audioCache,
                                     System::Status::StatusIndicator& status,
                                     const Profiles::SoundFont& font)
    : InertialEffect(0)
    , m_power(power)
    , m_audio(audio)
    , m_audioCache(audioCache)
    , m_status(status)
    , m_font(font)
{}

bool PreloadWaitEffect::test(const Core::SaberDataPacket&) {
    return m_power.state() == Profiles::PowerStateMachine::State::Locked;
}

void PreloadWaitEffect::run() {
    if (!m_audioCache.isPreloadComplete()) {
        m_status.show(SystemStatus::Preloading);
        return;
    }

    m_power.handle(Profiles::PowerStateMachine::Event::PreloadDone);

    m_status.show(SystemStatus::Ready);

    const std::string fontPath = m_font.selectionPath();
    ESP_LOGI(TAG, "Preload complete. Playing selection sound: %s", fontPath.c_str());
    m_audio.play(fontPath, false, kFullVolume);
}

} // namespace InertialSaber::Effects
