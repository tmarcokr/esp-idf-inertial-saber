#pragma once

#include "core/SaberActionBus.hpp"
#include "profiles/ProfileManager.hpp"
#include "profiles/SaberServices.hpp"

#include "system/adapters/ImuAdapter.hpp"
#include "system/adapters/InputAdapter.hpp"
#include "system/board/Board.hpp"
#include "system/hardware/HardwareConfig.hpp"
#include "system/PsramAudioCache.hpp"

#include "AudioEngine.hpp"
#include "Engine.hpp"
#include "GpioButton.hpp"
#include "Mpu6050.hpp"
#include "sd_card.hpp"

#include "esp_err.h"

namespace InertialSaber::System {

class SaberSystem {
public:
  SaberSystem() = default;
  ~SaberSystem() = default;

  SaberSystem(const SaberSystem &) = delete;
  SaberSystem &operator=(const SaberSystem &) = delete;

  [[nodiscard]] esp_err_t start();

private:
  [[nodiscard]] esp_err_t internalStart();
  [[nodiscard]] esp_err_t bringUpAudio();
  [[nodiscard]] esp_err_t bringUpBlade();

  // Warning: later members hold references to earlier ones; do not reorder.
  Board::StatusIndicatorType m_status{Board::kStatusIndicatorConfig};
  Espressif::Wrappers::SdCard m_sdCard{Hardware::makeSdConfig()};
  Espressif::Wrappers::Audio::AudioEngine m_audio{Hardware::makeAudioConfig()};
  Espressif::Wrappers::SmartLed::Engine m_blade{Board::kPins.bladeData, Hardware::HardwareConfig::kNumLeds};
  Espressif::Wrappers::Sensors::Mpu6050 m_imu{Board::kPins.imuSda, Board::kPins.imuScl, Board::kPins.imuInt};
  Espressif::Wrappers::GpioButton m_button{Board::kPins.mainButton, Board::kMainButtonActiveLow};
  Core::SaberActionBus m_bus{Hardware::HardwareConfig::kBusConfig};
  PsramAudioCache m_audioCache;
  Profiles::SaberServices m_services{m_bus, m_audio, m_blade, m_audioCache, m_status};
  Profiles::ProfileManager m_profiles{m_services};
  Adapters::ImuAdapter m_imuAdapter{m_bus, m_imu, Board::kPins.imuInt};
  Adapters::InputAdapter m_inputAdapter{m_bus, m_button};
};

} // namespace InertialSaber::System
