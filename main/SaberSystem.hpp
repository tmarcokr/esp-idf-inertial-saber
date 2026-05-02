#pragma once

#include "GpioButton.hpp"
#include "Mpu6050.hpp"
#include "SaberActionBus.hpp"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace InertialSaber {

/**
 * @brief Top-level system orchestrator for InertialSaber OS.
 *
 * Owns all hardware peripherals (IMU, buttons), the SaberAction Bus,
 * and the adapter tasks that translate raw hardware events into bus-
 * compatible data. Keeps main.cpp clean — single instantiation,
 * single start() call.
 */
class SaberSystem {
public:
    SaberSystem();
    ~SaberSystem();

    SaberSystem(const SaberSystem&) = delete;
    SaberSystem& operator=(const SaberSystem&) = delete;

    /**
     * @brief Initialize all hardware, register demo effects, and start the bus.
     * @return ESP_OK on success.
     */
    [[nodiscard]] esp_err_t start();

private:
    // ── Pin assignments (matching PoC wiring) ──
    static constexpr gpio_num_t kImuSda  = GPIO_NUM_22;
    static constexpr gpio_num_t kImuScl  = GPIO_NUM_23;
    static constexpr gpio_num_t kImuInt  = GPIO_NUM_21;
    static constexpr gpio_num_t kMainBtn = GPIO_NUM_9;

    static constexpr uint8_t kMainBtnInputId = 0;

    // ── Hardware ──
    Espressif::Wrappers::Sensors::Mpu6050 m_imu;
    Espressif::Wrappers::GpioButton m_mainButton;

    // ── Core ──
    Core::SaberActionBus m_bus;

    // ── IMU adapter task ──
    TaskHandle_t m_imuTaskHandle = nullptr;

    // ── Button state tracking ──
    Core::InputDescriptor m_btnState{};

    void registerDemoEffects();
    void setupButtonAdapter();

    static void IRAM_ATTR imuIsrHandler(void* arg);
    static void imuAdapterTask(void* arg);
    void imuLoop();
};

} // namespace InertialSaber
