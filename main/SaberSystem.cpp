#include "SaberSystem.hpp"

#include "ButtonActionEffect.hpp"
#include "MotionLogEffect.hpp"

#include "esp_log.h"
#include "esp_timer.h"

#include <cmath>
#include <memory>

namespace InertialSaber {

static constexpr const char* TAG = "SaberSystem";

SaberSystem::SaberSystem()
    : m_imu(kImuSda, kImuScl, kImuInt)
    , m_mainButton(kMainBtn, true) {}

SaberSystem::~SaberSystem() {
    if (m_imuTaskHandle != nullptr) {
        vTaskDelete(m_imuTaskHandle);
    }
}

esp_err_t SaberSystem::start() {
    ESP_LOGI(TAG, "Initializing InertialSaber OS...");

    // ── IMU ──
    esp_err_t err = m_imu.initialize();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "IMU initialization failed");
        return err;
    }
    ESP_LOGI(TAG, "IMU ready");

    // ── Button ──
    setupButtonAdapter();
    err = m_mainButton.init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Button initialization failed");
        return err;
    }
    ESP_LOGI(TAG, "Button ready (GPIO %d)", kMainBtn);

    // ── Effects ──
    registerDemoEffects();

    // ── Bus ──
    err = m_bus.start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Bus start failed");
        return err;
    }

    // ── IMU adapter task ──
    BaseType_t result = xTaskCreatePinnedToCore(
        imuAdapterTask,
        "imu_adapter",
        4096,
        this,
        Core::Platform::kBusTaskPriority + 1,
        &m_imuTaskHandle,
        Core::Platform::kBusTaskCore
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "IMU adapter task creation failed");
        return ESP_FAIL;
    }

    // ── IMU Interrupt Configuration ──
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << kImuInt),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&io_conf);
    
    // Install ISR service (ignoring INVALID_STATE if already installed by GpioButton)
    esp_err_t isr_err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(isr_err));
    }
    gpio_isr_handler_add(kImuInt, imuIsrHandler, this);

    ESP_LOGI(TAG, "InertialSaber OS active — all systems nominal");
    return ESP_OK;
}

void SaberSystem::registerDemoEffects() {
    m_bus.registerEffect(std::make_unique<Effects::MotionLogEffect>(500));
    m_bus.registerEffect(std::make_unique<Effects::ButtonActionEffect>(kMainBtnInputId));
    ESP_LOGI(TAG, "Demo effects registered");
}

void SaberSystem::setupButtonAdapter() {
    m_mainButton.onEvent(Espressif::Wrappers::ButtonEvent::PressDown, [this]() {
        m_btnState.previous = m_btnState.current;
        m_btnState.current = Core::InputDescriptor::State::PRESSED;
        m_btnState.lastTransition_ms = esp_timer_get_time() / 1000;
        m_btnState.pressCount++;
        m_bus.pushInputEvent(kMainBtnInputId, m_btnState);
    });

    m_mainButton.onEvent(Espressif::Wrappers::ButtonEvent::PressUp, [this]() {
        uint32_t now = esp_timer_get_time() / 1000;
        m_btnState.previous = m_btnState.current;
        m_btnState.current = Core::InputDescriptor::State::RELEASED;
        m_btnState.holdDuration_ms = now - m_btnState.lastTransition_ms;
        m_btnState.lastTransition_ms = now;
        m_bus.pushInputEvent(kMainBtnInputId, m_btnState);
    });
}

void SaberSystem::imuIsrHandler(void* arg) {
    auto* sys = static_cast<SaberSystem*>(arg);
    BaseType_t highTaskWoken = pdFALSE;
    if (sys->m_imuTaskHandle != nullptr) {
        vTaskNotifyGiveFromISR(sys->m_imuTaskHandle, &highTaskWoken);
        if (highTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

void SaberSystem::imuAdapterTask(void* arg) {
    auto* sys = static_cast<SaberSystem*>(arg);
    sys->imuLoop();
    vTaskDelete(nullptr);
}

void SaberSystem::imuLoop() {
    // Stabilization delay — let DMP fill initial FIFO
    vTaskDelay(pdMS_TO_TICKS(100));

    while (true) {
        // Wait for DMP interrupt notification. Timeout 20ms acts as a fallback
        // in case the INT pulse is missed, stuck, or the hardware wire is disconnected.
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));

        auto data = m_imu.readData();
        if (data) {
            auto linAccel = data->getLinearAcceleration();
            float energy = std::sqrt(
                linAccel.x * linAccel.x +
                linAccel.y * linAccel.y +
                linAccel.z * linAccel.z
            );

            float rotation[3] = {
                static_cast<float>(data->gyro_x),
                static_cast<float>(data->gyro_y),
                static_cast<float>(data->gyro_z)
            };

            auto angles = data->getEulerAngles();
            float orientation = angles.roll * (180.0f / M_PI);

            m_bus.updateMotion(energy, rotation, orientation);
        }
    }
}

} // namespace InertialSaber
