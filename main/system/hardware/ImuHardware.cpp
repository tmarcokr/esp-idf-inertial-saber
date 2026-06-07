#include "ImuHardware.hpp"
#include "system/hardware/HardwareConfig.hpp"
#include "esp_log.h"

namespace InertialSaber::System::Hardware {
static constexpr const char* TAG = "ImuHardware";

esp_err_t ImuHardware::init() {
    m_imu = std::make_unique<Espressif::Wrappers::Sensors::Mpu6050>(
        Hardware::HardwareConfig::kImuSda, Hardware::HardwareConfig::kImuScl, Hardware::HardwareConfig::kImuInt);
    esp_err_t err = m_imu->initialize();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "IMU initialization failed");
        return err;
    }
    ESP_LOGI(TAG, "IMU ready");
    return ESP_OK;
}
}
