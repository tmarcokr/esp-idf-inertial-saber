#pragma once
#include "Mpu6050.hpp"
#include <memory>
#include "esp_err.h"

namespace InertialSaber::System::Hardware {
class ImuHardware {
public:
    esp_err_t init();
    Espressif::Wrappers::Sensors::Mpu6050* getMpu() { return m_imu.get(); }
private:
    std::unique_ptr<Espressif::Wrappers::Sensors::Mpu6050> m_imu;
};
}
