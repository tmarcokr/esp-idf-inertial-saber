#include "SaberSystem.hpp"

#include "InertialBurstLogEffect.hpp"
#include "InertialSwingEffect.hpp"
#include "MotionLogEffect.hpp"
#include "PowerToggleEffect.hpp"

#include "esp_log.h"
#include "esp_timer.h"

#include <cmath>
#include <memory>

namespace InertialSaber {

static constexpr const char *TAG = "SaberSystem";

SaberSystem::SaberSystem()
    : m_imu(kImuSda, kImuScl, kImuInt), m_mainButton(kMainBtn, true) {}

SaberSystem::~SaberSystem() {
  if (m_imuTaskHandle != nullptr) {
    vTaskDelete(m_imuTaskHandle);
  }
}

esp_err_t SaberSystem::start() {
  ESP_LOGI(TAG, "Initializing InertialSaber OS...");

  // ── SD Card ──
  esp_err_t err = initSdCard();
  if (err != ESP_OK)
    return err;

  // ── Audio Engine ──
  err = initAudioEngine();
  if (err != ESP_OK)
    return err;

  // ── SmartLed Engine ──
  err = initLedEngine();
  if (err != ESP_OK)
    return err;

  // ── IMU ──
  err = m_imu.initialize();
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
  registerEffects();

  // ── Bus ──
  err = m_bus.start();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Bus start failed");
    return err;
  }

  // ── IMU adapter task ──
  BaseType_t result =
      xTaskCreatePinnedToCore(imuAdapterTask, "imu_adapter", 4096, this,
                              Core::Platform::kBusTaskPriority + 1,
                              &m_imuTaskHandle, Core::Platform::kBusTaskCore);

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

  esp_err_t isr_err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s",
             esp_err_to_name(isr_err));
  }
  gpio_isr_handler_add(kImuInt, imuIsrHandler, this);

  ESP_LOGI(TAG, "InertialSaber OS active — all systems nominal");
  return ESP_OK;
}

esp_err_t SaberSystem::initSdCard() {
  Espressif::Wrappers::SdCard::Config sd_cfg = {.miso = kSdMiso,
                                                .mosi = kSdMosi,
                                                .sck = kSdSck,
                                                .cs = kSdCs,
                                                .mount_point = "/sdcard",
                                                .max_files = 5,
                                                .format_if_mount_failed =
                                                    false};

  m_sdCard = std::make_unique<Espressif::Wrappers::SdCard>(sd_cfg);
  esp_err_t err = m_sdCard->init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "SD Card init failed: %s", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(TAG, "SD Card ready");
  return ESP_OK;
}

esp_err_t SaberSystem::initAudioEngine() {
  Espressif::Wrappers::Audio::AudioEngine::Config audio_cfg = {
      .bclk_pin = kI2sBclk,
      .ws_pin = kI2sWs,
      .dout_pin = kI2sDout,
      .sd_mode_pin = kI2sSdMode,
      .sample_rate = 44100,
      .max_channels = 9};

  m_audioEngine =
      std::make_unique<Espressif::Wrappers::Audio::AudioEngine>(audio_cfg);
  esp_err_t err = m_audioEngine->init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "AudioEngine init failed: %s", esp_err_to_name(err));
    return err;
  }

  err = m_audioEngine->start();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "AudioEngine start failed: %s", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(TAG, "Audio Engine ready (9 channels, 44.1kHz)");
  return ESP_OK;
}

esp_err_t SaberSystem::initLedEngine() {
  m_ledEngine = std::make_unique<Espressif::Wrappers::SmartLed::Engine>(
      kLedData, kNumLeds);
  esp_err_t err = m_ledEngine->init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "SmartLed init failed: %s", esp_err_to_name(err));
    return err;
  }
  m_ledEngine->setGlobalBrightness(255);
  // Target 100 FPS for responsive HSB changes
  m_ledEngine->setTargetFps(100);
  m_ledEngine->start();
  ESP_LOGI(TAG, "SmartLed Engine ready (%d LEDs on GPIO %d)", kNumLeds,
           kLedData);
  return ESP_OK;
}

void SaberSystem::registerEffects() {
  Effects::InertialSwing::SwingFontConfig fontConfig{
      .basePath = kFontBasePath,
      .humCount = Core::Platform::kFontHumCount,
      .swingPairCount = Core::Platform::kFontSwingPairCount,
      .burstCount = Core::Platform::kFontBurstCount};

  auto swingEffect = std::make_unique<Effects::InertialSwingEffect>(*m_audioEngine, fontConfig);
  auto *swingPtr = swingEffect.get();
  m_bus.registerEffect(std::move(swingEffect));

  auto lightEffect = std::make_unique<Effects::InertialLightEffect>(*m_ledEngine);
  auto *lightPtr = lightEffect.get();
  m_bus.registerEffect(std::move(lightEffect));

  m_bus.registerEffect(
      std::make_unique<Effects::PowerToggleEffect>(*swingPtr, *lightPtr, kMainBtnInputId));

  //   m_bus.registerEffect(std::make_unique<Effects::MotionLogEffect>(500));
  //   m_bus.registerEffect(std::make_unique<Effects::InertialBurstLogEffect>());

  ESP_LOGI(TAG,
           "Effects registered: InertialSwing, InertialLight, PowerToggle");
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

void SaberSystem::imuIsrHandler(void *arg) {
  auto *sys = static_cast<SaberSystem *>(arg);
  BaseType_t highTaskWoken = pdFALSE;
  if (sys->m_imuTaskHandle != nullptr) {
    vTaskNotifyGiveFromISR(sys->m_imuTaskHandle, &highTaskWoken);
    if (highTaskWoken == pdTRUE) {
      portYIELD_FROM_ISR();
    }
  }
}

void SaberSystem::imuAdapterTask(void *arg) {
  auto *sys = static_cast<SaberSystem *>(arg);
  sys->imuLoop();
  vTaskDelete(nullptr);
}

void SaberSystem::imuLoop() {
  vTaskDelay(pdMS_TO_TICKS(100));

  while (true) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));

    auto data = m_imu.readData();
    if (data) {
      auto linAccel = data->getLinearAcceleration();
      float energy =
          std::sqrt(linAccel.x * linAccel.x + linAccel.y * linAccel.y +
                    linAccel.z * linAccel.z);

      float rotation[3] = {static_cast<float>(data->gyro_x),
                           static_cast<float>(data->gyro_y),
                           static_cast<float>(data->gyro_z)};

      auto angles = data->getEulerAngles();
      float orientation = angles.roll * (180.0f / M_PI);

      m_bus.updateMotion(energy, rotation, orientation);
    }
  }
}

} // namespace InertialSaber
