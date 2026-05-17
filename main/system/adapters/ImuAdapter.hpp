#pragma once

#include "Mpu6050.hpp"
#include "core/bus/SaberActionBus.hpp"
#include "system/config/HardwareConfig.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

namespace InertialSaber::System::Adapters {

class ImuAdapter {
public:
    explicit ImuAdapter(Core::SaberActionBus& bus, Espressif::Wrappers::Sensors::Mpu6050& imu);
    ~ImuAdapter();

    ImuAdapter(const ImuAdapter&) = delete;
    ImuAdapter& operator=(const ImuAdapter&) = delete;

    [[nodiscard]] esp_err_t start();

private:
    Core::SaberActionBus& m_bus;
    Espressif::Wrappers::Sensors::Mpu6050& m_imu;
    TaskHandle_t m_imuTaskHandle = nullptr;

    static void IRAM_ATTR imuIsrHandler(void* arg);
    static void imuAdapterTask(void* arg);
    void imuLoop();
};

} // namespace InertialSaber::System::Adapters
