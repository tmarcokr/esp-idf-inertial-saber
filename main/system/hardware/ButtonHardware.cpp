#include "ButtonHardware.hpp"
#include "system/config/HardwareConfig.hpp"
#include "esp_log.h"

namespace InertialSaber::System::Hardware {
static constexpr const char* TAG = "ButtonHardware";

esp_err_t ButtonHardware::init() {
    m_button = std::make_unique<Espressif::Wrappers::GpioButton>(
        Config::HardwareConfig::kMainBtn, true);
    esp_err_t err = m_button->init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Button initialization failed");
        return err;
    }
    ESP_LOGI(TAG, "Button ready (GPIO %d)", Config::HardwareConfig::kMainBtn);
    return ESP_OK;
}
}
