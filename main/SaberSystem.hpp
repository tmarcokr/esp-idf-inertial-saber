#pragma once

#include "AudioEngine.hpp"
#include "Engine.hpp"
#include "GpioButton.hpp"
#include "InertialLightEffect.hpp"
#include "InertialSwingEffect.hpp"
#include "Mpu6050.hpp"
#include "SaberActionBus.hpp"
#include "sd_card.hpp"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <memory>

namespace InertialSaber {

/**
 * @brief Top-level system orchestrator for InertialSaber OS.
 *
 * Owns all hardware peripherals (IMU, buttons, SD card, audio engine),
 * the SaberAction Bus, and the adapter tasks that translate raw hardware
 * events into bus-compatible data. Keeps main.cpp clean — single
 * instantiation, single start() call.
 */
class SaberSystem {
public:
  SaberSystem();
  ~SaberSystem();

  SaberSystem(const SaberSystem &) = delete;
  SaberSystem &operator=(const SaberSystem &) = delete;

  /**
   * @brief Initialize all hardware, register effects, and start the bus.
   * @return ESP_OK on success.
   */
  [[nodiscard]] esp_err_t start();

private:
  // ── Pin assignments (matching PoC wiring) ──
  static constexpr gpio_num_t kImuSda = GPIO_NUM_22;
  static constexpr gpio_num_t kImuScl = GPIO_NUM_23;
  static constexpr gpio_num_t kImuInt = GPIO_NUM_21;
  static constexpr gpio_num_t kMainBtn = GPIO_NUM_9;

  // ── SD Card SPI pins ──
  static constexpr gpio_num_t kSdMiso = GPIO_NUM_4;
  static constexpr gpio_num_t kSdMosi = GPIO_NUM_11;
  static constexpr gpio_num_t kSdSck = GPIO_NUM_7;
  static constexpr gpio_num_t kSdCs = GPIO_NUM_10;

  // ── I2S / MAX98357A pins ──
  static constexpr gpio_num_t kI2sBclk = GPIO_NUM_18;
  static constexpr gpio_num_t kI2sWs = GPIO_NUM_19;
  static constexpr gpio_num_t kI2sDout = GPIO_NUM_20;
  static constexpr gpio_num_t kI2sSdMode = GPIO_NUM_1;

  // ── SmartLed pins ──
  static constexpr gpio_num_t kLedData = GPIO_NUM_0;
  static constexpr uint16_t kNumLeds = 5;

  static constexpr uint8_t kMainBtnInputId = 0;
  static constexpr const char *kFontBasePath = "/sdcard/InertialFont";

  // ── Hardware ──
  Espressif::Wrappers::Sensors::Mpu6050 m_imu;
  Espressif::Wrappers::GpioButton m_mainButton;
  std::unique_ptr<Espressif::Wrappers::SdCard> m_sdCard;
  std::unique_ptr<Espressif::Wrappers::Audio::AudioEngine> m_audioEngine;
  std::unique_ptr<Espressif::Wrappers::SmartLed::Engine> m_ledEngine;

  // ── Core ──
  Core::SaberActionBus m_bus;

  // ── IMU adapter task ──
  TaskHandle_t m_imuTaskHandle = nullptr;

  // ── Button state tracking ──
  Core::InputDescriptor m_btnState{};

  [[nodiscard]] esp_err_t initSdCard();
  [[nodiscard]] esp_err_t initAudioEngine();
  [[nodiscard]] esp_err_t initLedEngine();
  void registerEffects();
  void setupButtonAdapter();

  static void IRAM_ATTR imuIsrHandler(void *arg);
  static void imuAdapterTask(void *arg);
  void imuLoop();
};

} // namespace InertialSaber
