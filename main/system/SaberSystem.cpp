#include "SaberSystem.hpp"
#include "profiles/ProfileParser.hpp"
#include "esp_log.h"

namespace InertialSaber::System {

static constexpr const char *TAG = "SaberSystem";

using Status::SystemStatus;

esp_err_t SaberSystem::start() {
  ESP_LOGD(TAG, "sizeof(SaberSystem) = %u", static_cast<unsigned>(sizeof(SaberSystem)));

  if (m_status.init() == ESP_OK) {
    m_status.show(SystemStatus::Booting);
  }

  const esp_err_t err = internalStart();

  if (err != ESP_OK) {
    m_status.show(SystemStatus::Error);
  }
  return err;
}

esp_err_t SaberSystem::internalStart() {
  esp_err_t err;

#ifndef NDEBUG
  if ((err = Profiles::ProfileParser::runSelfTest()) != ESP_OK) return err;
#endif

  ESP_LOGI(TAG, "Initializing InertialSaber OS Hardware...");
  ESP_LOGI(TAG, "Board: %.*s", static_cast<int>(Board::kName.size()), Board::kName.data());

  if ((err = m_sdCard.init()) != ESP_OK) {
    ESP_LOGE(TAG, "SD Card init failed: %s", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(TAG, "SD Card ready");

  if ((err = bringUpAudio()) != ESP_OK) return err;
  if ((err = bringUpBlade()) != ESP_OK) return err;

  if ((err = m_imu.initialize()) != ESP_OK) {
    ESP_LOGE(TAG, "IMU initialization failed");
    return err;
  }
  ESP_LOGI(TAG, "IMU ready");

  if ((err = m_audioCache.init()) != ESP_OK) return err;

  ESP_LOGI(TAG, "Starting Adapters...");
  if ((err = m_imuAdapter.start()) != ESP_OK) return err;
  if ((err = m_inputAdapter.start()) != ESP_OK) return err;

  // Warning: GpioButton::init() starts the poll task that iterates the callback
  // maps unlocked, so all callbacks must be registered before it.
  if ((err = m_button.init()) != ESP_OK) {
    ESP_LOGE(TAG, "Button initialization failed");
    return err;
  }
  ESP_LOGI(TAG, "Button ready (GPIO %d)", static_cast<int>(Board::kPins.mainButton));

  ESP_LOGI(TAG, "Loading Profiles...");
  m_profiles.init();
  m_profiles.loadActive();

  ESP_LOGI(TAG, "Starting Action Bus...");
  if ((err = m_bus.start()) != ESP_OK) {
    ESP_LOGE(TAG, "Bus start failed");
    return err;
  }

  ESP_LOGI(TAG, "InertialSaber OS active — all systems nominal");
  return ESP_OK;
}

esp_err_t SaberSystem::bringUpAudio() {
  esp_err_t err = m_audio.init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "AudioEngine init failed: %s", esp_err_to_name(err));
    return err;
  }

  err = m_audio.start();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "AudioEngine start failed: %s", esp_err_to_name(err));
    return err;
  }

  m_audio.setGlobalVolume(Hardware::HardwareConfig::kAudioGlobalVolume);

  ESP_LOGI(TAG, "Audio Engine ready (9 channels, 44.1kHz)");
  return ESP_OK;
}

esp_err_t SaberSystem::bringUpBlade() {
  const esp_err_t err = m_blade.init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "SmartLed init failed: %s", esp_err_to_name(err));
    return err;
  }

  m_blade.setGlobalBrightness(Hardware::HardwareConfig::kBladeBrightness);
  m_blade.setTargetFps(Hardware::HardwareConfig::kBladeTargetFps);
  m_blade.start();

  ESP_LOGI(TAG, "SmartLed Engine ready (%d LEDs on GPIO %d)",
           Hardware::HardwareConfig::kNumLeds, static_cast<int>(Board::kPins.bladeData));
  return ESP_OK;
}

} // namespace InertialSaber::System
