// SPDX-License-Identifier: GPL-3.0-or-later

#include "ImuAdapter.hpp"
#include "diagnostics/Metrics.hpp"
#include "system/hardware/HardwareConfig.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace InertialSaber::System::Adapters {

static constexpr const char* TAG = "ImuAdapter";
static constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;
static constexpr const Hardware::TaskSpec& kTask = Hardware::TaskTable::kImuAdapter;
static constexpr uint32_t kStartupDelayMs = 100;
static constexpr uint32_t kPollTimeoutMs = 20;
static constexpr int64_t kUsPerMs = 1000;
static constexpr float kMilliGPerG = 1000.0f;
static constexpr float kGravityG = 1.0f;

// MPU-6000/MPU-6050 Product Specification rev 3.4, sections 6.1 and 6.2: sensitivities at the full
// scales selected by the Mpu6050 component (FS_SEL = 3, AFS_SEL = 0).
static constexpr float kGyroLsbPerDps = 16.4f;
static constexpr float kAccelLsbPerG = 16384.0f;

static constexpr float kQuasiStaticMaxAccelDeviationG = 0.08f;
static constexpr float kQuasiStaticMaxRateDps = 20.0f;
static constexpr float kSettledMaxGravityAngleDeg = 4.0f;
static constexpr uint32_t kSettledMinAlignedSamples = 20;
static constexpr int64_t kSettleCeilingMs = 20'000;
static constexpr int64_t kSettleBackstopMs = 2 * kSettleCeilingMs;

namespace {

using Espressif::Wrappers::Sensors::MotionData;

float magnitude(const MotionData::Vector3D& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

MotionData::Vector3D rawAccelerationG(const MotionData& data) {
    return {.x = static_cast<float>(data.accel_x) / kAccelLsbPerG,
            .y = static_cast<float>(data.accel_y) / kAccelLsbPerG,
            .z = static_cast<float>(data.accel_z) / kAccelLsbPerG};
}

bool isQuasiStatic(const MotionData& data, float accelMagnitudeG) {
    const auto rateBelowLimit = [](int16_t rawRate) {
        return std::fabs(static_cast<float>(rawRate)) / kGyroLsbPerDps < kQuasiStaticMaxRateDps;
    };
    return std::fabs(accelMagnitudeG - kGravityG) < kQuasiStaticMaxAccelDeviationG &&
           rateBelowLimit(data.gyro_x) && rateBelowLimit(data.gyro_y) &&
           rateBelowLimit(data.gyro_z);
}

bool isAlignedWithDmpGravity(const MotionData& data, const MotionData::Vector3D& accelG,
                             float accelMagnitudeG) {
    const MotionData::Vector3D gravity = data.getGravityVector();
    const float norms = magnitude(gravity) * accelMagnitudeG;
    if (norms <= 0.0f) return false;
    const float cosine =
        (gravity.x * accelG.x + gravity.y * accelG.y + gravity.z * accelG.z) / norms;
    return std::acos(std::clamp(cosine, -1.0f, 1.0f)) * kRadToDeg < kSettledMaxGravityAngleDeg;
}

float tiltRollDeg(const MotionData::Vector3D& accelG) {
    return std::atan2(accelG.y, std::sqrt(accelG.x * accelG.x + accelG.z * accelG.z)) * kRadToDeg;
}

} // namespace

ImuAdapter::ImuAdapter(Core::SaberActionBus& bus, Espressif::Wrappers::Sensors::Mpu6050& imu,
                       gpio_num_t interruptPin)
    : m_bus(bus)
    , m_imu(imu)
    , m_interruptPin(interruptPin) {}

ImuAdapter::~ImuAdapter() {
    if (m_isrHandlerAdded) {
        (void)gpio_isr_handler_remove(m_interruptPin);
    }
    if (m_imuTaskHandle != nullptr) {
        vTaskDelete(m_imuTaskHandle);
    }
}

esp_err_t ImuAdapter::start() {
    BaseType_t result = xTaskCreatePinnedToCore(imuAdapterTask, kTask.name, kTask.stackSize, this,
                                                kTask.priority, &m_imuTaskHandle, kTask.core);

    if (result != pdPASS) {
        ESP_LOGE(TAG, "IMU adapter task creation failed");
        return ESP_FAIL;
    }
    SABER_METRIC_REGISTER_TASK(Diagnostics::TaskId::Imu, m_imuTaskHandle);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << m_interruptPin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    const esp_err_t config_err = gpio_config(&io_conf);
    if (config_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure IMU interrupt pin: %s", esp_err_to_name(config_err));
    }

    esp_err_t isr_err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(isr_err));
    }
    const esp_err_t add_err = gpio_isr_handler_add(m_interruptPin, imuIsrHandler, this);
    if (add_err == ESP_OK) {
        m_isrHandlerAdded = true;
    } else {
        ESP_LOGE(TAG, "Failed to add IMU ISR handler: %s", esp_err_to_name(add_err));
    }
    if (config_err != ESP_OK || !m_isrHandlerAdded) {
        ESP_LOGW(TAG, "IMU interrupt unavailable, polling every %lu ms",
                 static_cast<unsigned long>(kPollTimeoutMs));
    }

    ESP_LOGI(TAG, "IMU Adapter started successfully");
    return ESP_OK;
}

void IRAM_ATTR ImuAdapter::imuIsrHandler(void* arg) {
    auto* self = static_cast<ImuAdapter*>(arg);
    BaseType_t highTaskWoken = pdFALSE;
    if (self->m_imuTaskHandle != nullptr) {
        vTaskNotifyGiveFromISR(self->m_imuTaskHandle, &highTaskWoken);
        if (highTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

void ImuAdapter::imuAdapterTask(void* arg) {
    auto* self = static_cast<ImuAdapter*>(arg);
    self->imuLoop();
    vTaskDelete(nullptr);
}

void ImuAdapter::imuLoop() {
    vTaskDelay(pdMS_TO_TICKS(kStartupDelayMs));

    while (true) {
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kPollTimeoutMs)) == 0) {
            SABER_METRIC_COUNT(Diagnostics::Counter::ImuPollTimeouts);
        }

        auto data = readMotion();
        if (!data) {
            SABER_METRIC_COUNT(Diagnostics::Counter::ImuEmptyReads);
        } else {
            const int64_t sampleTimeUs = esp_timer_get_time();
            SABER_METRIC_COUNT(Diagnostics::Counter::ImuSamples);
            const MotionEstimate estimate = estimateMotion(*data, sampleTimeUs);

            const Core::MotionSample sample{
                .kineticEnergyG = estimate.kineticEnergyG,
                .axisRotationDps = {static_cast<float>(data->gyro_x),
                                    static_cast<float>(data->gyro_y),
                                    static_cast<float>(data->gyro_z)},
                .orientationDeg = estimate.orientationDeg,
                .timestampUs = sampleTimeUs,
            };

            m_bus.updateMotion(sample);
        }
    }
}

std::optional<MotionData> ImuAdapter::readMotion() {
    SABER_METRIC_SCOPE(Diagnostics::Metric::ImuRead);
    return m_imu.readData();
}

ImuAdapter::MotionEstimate ImuAdapter::estimateMotion(const MotionData& data,
                                                      int64_t sampleTimeUs) {
    const MotionData::Vector3D accelG = rawAccelerationG(data);
    const float accelMagnitudeG = magnitude(accelG);
    const bool quasiStatic = isQuasiStatic(data, accelMagnitudeG);

    if (!m_dmpSettled &&
        !updateDmpSettling(data, accelG, accelMagnitudeG, quasiStatic, sampleTimeUs)) {
        SABER_METRIC_COUNT(Diagnostics::Counter::ImuFallbackSamples);
        return {.kineticEnergyG = std::fabs(accelMagnitudeG - kGravityG),
                .orientationDeg = tiltRollDeg(accelG)};
    }

    const float kineticEnergyG = magnitude(data.getLinearAcceleration());
    if (quasiStatic) {
        SABER_METRIC_DURATION(Diagnostics::Metric::KineticEnergyQuasiStaticSettled,
                              static_cast<uint32_t>(kineticEnergyG * kMilliGPerG));
    }
    return {.kineticEnergyG = kineticEnergyG,
            .orientationDeg = data.getEulerAngles().roll * kRadToDeg};
}

bool ImuAdapter::updateDmpSettling(const MotionData& data, const MotionData::Vector3D& accelG,
                                   float accelMagnitudeG, bool quasiStatic, int64_t sampleTimeUs) {
    if (!m_firstSampleUs) {
        m_firstSampleUs = sampleTimeUs;
    }
    const int64_t elapsedUs = sampleTimeUs - *m_firstSampleUs;
    const auto settleMs = static_cast<int32_t>(sampleTimeUs / kUsPerMs);
    if (!quasiStatic) {
        m_alignedQuasiStaticSamples = 0;
        if (elapsedUs < kSettleBackstopMs * kUsPerMs) return false;

        m_dmpSettled = true;
        SABER_METRIC_IMU_SETTLE(Diagnostics::Metrics::kImuSettleForcedByBackstop);
        ESP_LOGI(TAG,
                 "No quasi-static IMU sample before the backstop; DMP motion values from %ld ms",
                 static_cast<long>(settleMs));
        return true;
    }

    if (isAlignedWithDmpGravity(data, accelG, accelMagnitudeG)) {
        ++m_alignedQuasiStaticSamples;
    } else {
        m_alignedQuasiStaticSamples = 0;
    }
    const bool aligned = m_alignedQuasiStaticSamples >= kSettledMinAlignedSamples;
    const bool ceilingReached = elapsedUs >= kSettleCeilingMs * kUsPerMs;
    if (!aligned && !ceilingReached) return false;

    m_dmpSettled = true;
    if (aligned) {
        SABER_METRIC_IMU_SETTLE(settleMs);
        ESP_LOGI(TAG, "DMP settled %ld ms after power-up", static_cast<long>(settleMs));
    } else {
        SABER_METRIC_IMU_SETTLE(Diagnostics::Metrics::kImuSettleForcedByCeiling);
        ESP_LOGI(TAG, "DMP settling ceiling reached; DMP motion values from %ld ms",
                 static_cast<long>(settleMs));
    }
    return true;
}

} // namespace InertialSaber::System::Adapters
