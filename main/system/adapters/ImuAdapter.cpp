#include "ImuAdapter.hpp"
#include "diagnostics/Metrics.hpp"
#include "system/hardware/HardwareConfig.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include <cmath>
#include <numbers>

namespace InertialSaber::System::Adapters {

static constexpr const char* TAG = "ImuAdapter";
static constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;
static constexpr uint32_t kTaskStackSize = 4096;
static constexpr uint32_t kStartupDelayMs = 100;
static constexpr uint32_t kPollTimeoutMs = 20;

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
    BaseType_t result =
        xTaskCreatePinnedToCore(imuAdapterTask, "imu_adapter", kTaskStackSize, this,
                                Hardware::HardwareConfig::kImuAdapterPriority, &m_imuTaskHandle,
                                Hardware::HardwareConfig::kImuAdapterCore);

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
            auto linAccel = data->getLinearAcceleration();
            float energy = std::sqrt(linAccel.x * linAccel.x + linAccel.y * linAccel.y +
                                     linAccel.z * linAccel.z);

            auto angles = data->getEulerAngles();
            float orientation = angles.roll * kRadToDeg;

            const Core::MotionSample sample{
                .kineticEnergyG = energy,
                .axisRotationDps = {static_cast<float>(data->gyro_x),
                                    static_cast<float>(data->gyro_y),
                                    static_cast<float>(data->gyro_z)},
                .orientationDeg = orientation,
                .timestampUs = sampleTimeUs,
            };

            m_bus.updateMotion(sample);
        }
    }
}

std::optional<Espressif::Wrappers::Sensors::MotionData> ImuAdapter::readMotion() {
    SABER_METRIC_SCOPE(Diagnostics::Metric::ImuRead);
    return m_imu.readData();
}

} // namespace InertialSaber::System::Adapters
