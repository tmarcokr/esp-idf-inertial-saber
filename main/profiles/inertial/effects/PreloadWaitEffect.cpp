#include "PreloadWaitEffect.hpp"
#include "diagnostics/Metrics.hpp"
#include "profiles/PowerStateMachine.hpp"
#include "profiles/SoundFont.hpp"
#include "AudioLevels.hpp"
#include "system/PsramAudioCache.hpp"
#include "system/status/StatusIndicator.hpp"
#include "system/audio/AudioController.hpp"
#include "esp_log.h"

namespace InertialSaber::Effects {

static constexpr const char* TAG = "PreloadWait";

using System::Status::SystemStatus;

PreloadWaitEffect::PreloadWaitEffect(Profiles::PowerStateMachine& power,
                                     System::AudioController& audio,
                                     const System::PsramAudioCache& audioCache,
                                     System::Status::StatusIndicator& status,
                                     const Profiles::SoundFont& font)
    : InertialEffect(0)
    , m_power(power)
    , m_audio(audio)
    , m_audioCache(audioCache)
    , m_status(status)
    , m_font(font) {}

bool PreloadWaitEffect::test(const Core::SaberDataPacket&) {
    return m_power.state() == Profiles::PowerStateMachine::State::Locked;
}

void PreloadWaitEffect::run() {
    SABER_METRIC_SCOPE(Diagnostics::Metric::RunPreloadWait);
    using PreloadStatus = System::PsramAudioCache::PreloadStatus;
    switch (m_audioCache.preloadStatus()) {
    case PreloadStatus::Pending:
        m_status.show(SystemStatus::Preloading);
        return;
    case PreloadStatus::Failed:
        m_power.handle(Profiles::PowerStateMachine::Event::PreloadFailed);
        m_status.show(SystemStatus::Error);
        ESP_LOGE(
            TAG,
            "Preload failed for '%s'. Ignition disabled; triple-click to reload or cycle profile.",
            m_font.root().c_str());
        return;
    case PreloadStatus::Ready:
        break;
    }

    m_power.handle(Profiles::PowerStateMachine::Event::PreloadDone);

    m_status.show(SystemStatus::Ready);

    const System::AudioPath fontPath = m_font.selectionPath();
    ESP_LOGD(TAG, "Preload complete. Playing selection sound: %s", fontPath.c_str());
    m_audio.playOneShot(fontPath, kFullVolume);
}

} // namespace InertialSaber::Effects
