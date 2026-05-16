#pragma once

#include "core/interfaces/InertialProfile.hpp"
#include "core/bus/SaberActionBus.hpp"
#include "profiles/inertial/InertialDefaultProfile.hpp"

#include "system/adapters/ImuAdapter.hpp"
#include "system/adapters/InputAdapter.hpp"

#include "system/hardware/AudioHardware.hpp"
#include "system/hardware/LedHardware.hpp"
#include "system/hardware/ImuHardware.hpp"
#include "system/hardware/ButtonHardware.hpp"
#include "system/hardware/SdHardware.hpp"

#include "esp_err.h"
#include <memory>

namespace InertialSaber::System {

class SaberSystem {
public:
  SaberSystem();
  ~SaberSystem() = default;

  SaberSystem(const SaberSystem &) = delete;
  SaberSystem &operator=(const SaberSystem &) = delete;

  [[nodiscard]] esp_err_t start();

private:
  // ── Hardware ──
  Hardware::SdHardware m_sdHardware;
  Hardware::AudioHardware m_audioHardware;
  Hardware::LedHardware m_ledHardware;
  Hardware::ImuHardware m_imuHardware;
  Hardware::ButtonHardware m_btnHardware;

  // ── Adapters ──
  std::unique_ptr<Adapters::ImuAdapter> m_imuAdapter;
  std::unique_ptr<Adapters::InputAdapter> m_inputAdapter;

  // ── Core ──
  Core::SaberActionBus m_bus;
  std::unique_ptr<Core::InertialProfile> m_profile;
};

} // namespace InertialSaber::System
