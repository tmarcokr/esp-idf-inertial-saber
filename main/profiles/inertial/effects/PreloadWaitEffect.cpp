#include "PreloadWaitEffect.hpp"
#include "profiles/ConfigurableProfile.hpp"
#include "AudioEngine.hpp"
#include "RgbLed.hpp"
#include "esp_log.h"
#include "esp_timer.h"

#if CONFIG_IDF_TARGET_ESP32S3
#include "system/PsramAudioCache.hpp"
#endif

namespace InertialSaber::Effects {

static constexpr const char* TAG = "PreloadWait";

PreloadWaitEffect::PreloadWaitEffect(
    Profiles::ConfigurableProfile&           profile,
    Espressif::Wrappers::Audio::AudioEngine& audio,
    Espressif::Wrappers::RgbLed*             statusLed
#if CONFIG_IDF_TARGET_ESP32S3
    , System::PsramAudioCache*               psramCache
#endif
)
    : m_profile(profile)
    , m_audio(audio)
    , m_statusLed(statusLed)
#if CONFIG_IDF_TARGET_ESP32S3
    , m_psramCache(psramCache)
#endif
{
    Priority = 0;
}

bool PreloadWaitEffect::Test(const Core::SaberDataPacket&) {
    return m_profile.getPowerState() == Profiles::ConfigurableProfile::PowerState::PRELOADING;
}

void PreloadWaitEffect::Run() {
#if CONFIG_IDF_TARGET_ESP32S3
    if (m_psramCache && !m_psramCache->isPreloadComplete()) {
        if (m_statusLed) {
            bool blinkOn = ((esp_timer_get_time() / 1000LL) / 250) % 2 == 0;
            if (blinkOn) {
                (void)m_statusLed->setColor({64, 64, 0});
            } else {
                (void)m_statusLed->clear();
            }
        }
        return;
    }
#endif

    m_profile.setPowerState(Profiles::ConfigurableProfile::PowerState::RETRACTED);

    if (m_statusLed) {
        (void)m_statusLed->setColor({0, 32, 0});
    }

    const auto& def = m_profile.getDefinition();
    std::string fontPath = std::string("/sdcard/") + def.profileRoot + "font.wav";
    ESP_LOGI(TAG, "Preload complete. Playing selection sound: %s", fontPath.c_str());
    m_audio.play(fontPath, false, 16384);
}

} // namespace InertialSaber::Effects
