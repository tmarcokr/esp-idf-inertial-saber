#include "AudioHardware.hpp"
#include "system/hardware/HardwareConfig.hpp"
#include "esp_log.h"

namespace InertialSaber::System::Hardware {
static constexpr const char* TAG = "AudioHardware";

esp_err_t AudioHardware::init() {
    Espressif::Wrappers::Audio::AudioEngine::Config audio_cfg = {
        .bclk_pin = Hardware::HardwareConfig::kI2sBclk,
        .ws_pin = Hardware::HardwareConfig::kI2sWs,
        .dout_pin = Hardware::HardwareConfig::kI2sDout,
        .sd_mode_pin = Hardware::HardwareConfig::kI2sSdMode,
        .sample_rate = 44100,
        .max_channels = 9,
        .compressor_gain_threshold = 1000,
        .dc_cutoff = Espressif::Wrappers::Audio::DcBlocker::CutoffPreset::Hz50};

    m_audioEngine = std::make_unique<Espressif::Wrappers::Audio::AudioEngine>(audio_cfg);
    esp_err_t err = m_audioEngine->init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AudioEngine init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = m_audioEngine->start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AudioEngine start failed: %s", esp_err_to_name(err));
        return err;
    }

    // Master volume at Q14 unity. The PolyphonicMixer now applies a sqrt-law
    // compressor (Kinetic Acoustic Compressor) + DC blocker, so loud/bass content
    // is gain-reduced before the final clamp instead of flat-topping.
    // The compressor_gain_threshold and dc_cutoff above are tuned for this specific hardware.
    m_audioEngine->setGlobalVolume(16384);

    ESP_LOGI(TAG, "Audio Engine ready (9 channels, 44.1kHz)");
    return ESP_OK;
}
}
