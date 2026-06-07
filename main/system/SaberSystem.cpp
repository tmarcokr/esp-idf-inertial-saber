#include "SaberSystem.hpp"
#include "profiles/ProfileParser.hpp"
#include "core/effects/inertial_engine/audio/InertialSwingEffect.hpp"
#include "profiles/inertial/effects/ProfileCycleEffect.hpp"
#include "system/config/HardwareConfig.hpp"
#include "esp_log.h"

namespace InertialSaber::System {

static constexpr const char *TAG = "SaberSystem";

SaberSystem::SaberSystem() {}

esp_err_t SaberSystem::start() {
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

  ESP_LOGI(TAG, "Starting Adapters...");
  m_imuAdapter = std::make_unique<Adapters::ImuAdapter>(m_bus, *m_imuHardware.getMpu());
  if ((err = m_imuAdapter->start()) != ESP_OK) return err;

  m_inputAdapter = std::make_unique<Adapters::InputAdapter>(m_bus, *m_btnHardware.getButton());
  if ((err = m_inputAdapter->start()) != ESP_OK) return err;

  ESP_LOGI(TAG, "Loading Profiles...");
  m_profileManager.init();
  m_profileManager.loadActive(m_bus, *m_audioHardware.getEngine(), *m_ledHardware.getEngine());

  m_bus.registerEffect(std::make_unique<Effects::ProfileCycleEffect>(
      m_profileManager.getActiveProfile(),
      m_profileManager,
      m_bus,
      *m_audioHardware.getEngine(),
      *m_ledHardware.getEngine(),
      Config::HardwareConfig::kMainBtnInputId));

  ESP_LOGI(TAG, "Starting Action Bus...");
  if ((err = m_bus.start()) != ESP_OK) {
    ESP_LOGE(TAG, "Bus start failed");
    return err;
  }

  ESP_LOGI(TAG, "InertialSaber OS active — all systems nominal");
  return ESP_OK;
}

} // namespace InertialSaber::System
