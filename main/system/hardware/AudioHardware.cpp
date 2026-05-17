#include "AudioHardware.hpp"
#include "system/config/HardwareConfig.hpp"
#include "esp_log.h"

namespace InertialSaber::System::Hardware {
static constexpr const char* TAG = "AudioHardware";

esp_err_t AudioHardware::init() {
    Espressif::Wrappers::Audio::AudioEngine::Config audio_cfg = {
        .bclk_pin = Config::HardwareConfig::kI2sBclk,
        .ws_pin = Config::HardwareConfig::kI2sWs,
        .dout_pin = Config::HardwareConfig::kI2sDout,
        .sd_mode_pin = Config::HardwareConfig::kI2sSdMode,
        .sample_rate = 44100,
        .max_channels = 9};

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
    ESP_LOGI(TAG, "Audio Engine ready (9 channels, 44.1kHz)");
    return ESP_OK;
}
}
