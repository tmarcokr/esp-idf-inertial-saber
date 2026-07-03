#include "SaberSystem.hpp"
#include "profiles/ProfileParser.hpp"
#include "profiles/inertial/effects/InertialSwingEffect.hpp"
#include "system/hardware/HardwareConfig.hpp"
#include "esp_log.h"

namespace InertialSaber::System {

static constexpr const char *TAG = "SaberSystem";

SaberSystem::SaberSystem() : m_statusLed(Hardware::HardwareConfig::kStatusLed) {}

esp_err_t SaberSystem::start() {
  esp_err_t err;
  
  if ((err = m_statusLed.init()) == ESP_OK) {
      (void)m_statusLed.setColor({128, 128, 0}); // Yellow: Booting (dim)
  }

  err = internalStart();

  if (err != ESP_OK) {
      (void)m_statusLed.setColor({255, 0, 0}); // Red: Error
  }
  return err;
}

esp_err_t SaberSystem::internalStart() {
  esp_err_t err;

#ifndef NDEBUG
  if ((err = Profiles::ProfileParser::runSelfTest()) != ESP_OK) return err;
#endif

  ESP_LOGI(TAG, "Initializing InertialSaber OS Hardware...");
  
  if ((err = m_sdHardware.init()) != ESP_OK) return err;
  if ((err = m_audioHardware.init()) != ESP_OK) return err;
  if ((err = m_ledHardware.init()) != ESP_OK) return err;
  if ((err = m_imuHardware.init()) != ESP_OK) return err;
  if ((err = m_btnHardware.init()) != ESP_OK) return err;

  if ((err = m_psramCache.init()) != ESP_OK) return err;
  m_profileManager.setPsramCache(&m_psramCache);


  ESP_LOGI(TAG, "Starting Adapters...");
  m_imuAdapter = std::make_unique<Adapters::ImuAdapter>(m_bus, *m_imuHardware.getMpu());
  if ((err = m_imuAdapter->start()) != ESP_OK) return err;

  m_inputAdapter = std::make_unique<Adapters::InputAdapter>(m_bus, *m_btnHardware.getButton());
  if ((err = m_inputAdapter->start()) != ESP_OK) return err;

  ESP_LOGI(TAG, "Loading Profiles...");
  m_profileManager.setStatusLed(&m_statusLed);
  m_profileManager.init();
  m_profileManager.loadActive(m_bus, *m_audioHardware.getEngine(), *m_ledHardware.getEngine());

  ESP_LOGI(TAG, "Starting Action Bus...");
  if ((err = m_bus.start()) != ESP_OK) {
    ESP_LOGE(TAG, "Bus start failed");
    return err;
  }

  ESP_LOGI(TAG, "InertialSaber OS active — all systems nominal");
  return ESP_OK;
}

} // namespace InertialSaber::System
