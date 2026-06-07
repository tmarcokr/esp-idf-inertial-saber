#include "LedHardware.hpp"
#include "system/hardware/HardwareConfig.hpp"
#include "esp_log.h"

namespace InertialSaber::System::Hardware {
static constexpr const char* TAG = "LedHardware";

esp_err_t LedHardware::init() {
    m_ledEngine = std::make_unique<Espressif::Wrappers::SmartLed::Engine>(
        Hardware::HardwareConfig::kLedData, Hardware::HardwareConfig::kNumLeds);
    esp_err_t err = m_ledEngine->init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SmartLed init failed: %s", esp_err_to_name(err));
        return err;
    }
    m_ledEngine->setGlobalBrightness(255);
    m_ledEngine->setTargetFps(100);
    m_ledEngine->start();
    ESP_LOGI(TAG, "SmartLed Engine ready (%d LEDs on GPIO %d)",
             Hardware::HardwareConfig::kNumLeds, Hardware::HardwareConfig::kLedData);
    return ESP_OK;
}
}
