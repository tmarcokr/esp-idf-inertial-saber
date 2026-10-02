#pragma once

#include "Mpu6050.hpp"
#include "core/SaberActionBus.hpp"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include <optional>

namespace InertialSaber::System::Adapters {

class ImuAdapter {
public:
    ImuAdapter(Core::SaberActionBus& bus, Espressif::Wrappers::Sensors::Mpu6050& imu,
               gpio_num_t interruptPin);
    ~ImuAdapter();

    ImuAdapter(const ImuAdapter&) = delete;
    ImuAdapter& operator=(const ImuAdapter&) = delete;

    [[nodiscard]] esp_err_t start();

private:
    using MotionData = Espressif::Wrappers::Sensors::MotionData;

    struct MotionEstimate {
        float kineticEnergyG;
        float orientationDeg;
    };

    Core::SaberActionBus& m_bus;
    Espressif::Wrappers::Sensors::Mpu6050& m_imu;
    gpio_num_t m_interruptPin;
    bool m_isrHandlerAdded = false;
    TaskHandle_t m_imuTaskHandle = nullptr;

    // Warning: the settling state below is owned by the IMU task; no other task may touch it.
    std::optional<int64_t> m_firstSampleUs;
    uint32_t m_alignedQuasiStaticSamples = 0;
    bool m_dmpSettled = false;

    static void IRAM_ATTR imuIsrHandler(void* arg);
    static void imuAdapterTask(void* arg);
    void imuLoop();
    std::optional<MotionData> readMotion();
    MotionEstimate estimateMotion(const MotionData& data, int64_t sampleTimeUs);
    bool updateDmpSettling(const MotionData& data, const MotionData::Vector3D& accelG,
                           float accelMagnitudeG, bool quasiStatic, int64_t sampleTimeUs);
};

} // namespace InertialSaber::System::Adapters
