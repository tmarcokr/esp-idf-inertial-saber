#include "ImuAdapter.hpp"
#include "esp_log.h"
#include "driver/gpio.h"
#include <cmath>

namespace InertialSaber::System::Adapters {

static constexpr const char* TAG = "ImuAdapter";

ImuAdapter::ImuAdapter(Core::SaberActionBus& bus, Espressif::Wrappers::Sensors::Mpu6050& imu,
                       gpio_num_t interruptPin)
    : m_bus(bus), m_imu(imu), m_interruptPin(interruptPin) {}

ImuAdapter::~ImuAdapter() {
    if (m_isrHandlerAdded) {
        (void)gpio_isr_handler_remove(m_interruptPin);
    }
    if (m_imuTaskHandle != nullptr) {
        vTaskDelete(m_imuTaskHandle);
    }
}

esp_err_t ImuAdapter::start() {
    BaseType_t result = xTaskCreatePinnedToCore(
        imuAdapterTask, "imu_adapter", 4096, this,
        Hardware::HardwareConfig::kImuAdapterPriority,
        &m_imuTaskHandle, Hardware::HardwareConfig::kImuAdapterCore);

    if (result != pdPASS) {
        ESP_LOGE(TAG, "IMU adapter task creation failed");
        return ESP_FAIL;
    }

    // ── IMU Interrupt Configuration ──
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << m_interruptPin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&io_conf);

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
    vTaskDelay(pdMS_TO_TICKS(100));

    while (true) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));

        auto data = m_imu.readData();
        if (data) {
            auto linAccel = data->getLinearAcceleration();
            float energy = std::sqrt(linAccel.x * linAccel.x +
                                     linAccel.y * linAccel.y +
                                     linAccel.z * linAccel.z);

            auto angles = data->getEulerAngles();
            float orientation = angles.roll * (180.0f / M_PI);

            const Core::MotionSample sample{
                .kineticEnergyG  = energy,
                .axisRotationDps = {static_cast<float>(data->gyro_x),
                                    static_cast<float>(data->gyro_y),
                                    static_cast<float>(data->gyro_z)},
                .orientationDeg  = orientation,
            };

            m_bus.updateMotion(sample);
        }
    }
}

} // namespace InertialSaber::System::Adapters
