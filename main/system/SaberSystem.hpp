#pragma once

#include "core/SaberActionBus.hpp"
#include "profiles/ProfileManager.hpp"

#include "system/adapters/ImuAdapter.hpp"
#include "system/adapters/InputAdapter.hpp"

#include "system/hardware/AudioHardware.hpp"
#include "system/hardware/LedHardware.hpp"
#include "system/hardware/ImuHardware.hpp"
#include "system/hardware/ButtonHardware.hpp"
#include "system/hardware/SdHardware.hpp"
#include "RgbLed.hpp"

#include "esp_err.h"
#include <memory>

#if CONFIG_IDF_TARGET_ESP32S3
#include "system/PsramAudioCache.hpp"
#endif

namespace InertialSaber::System {

class SaberSystem {
public:
  SaberSystem();
  ~SaberSystem() = default;

  SaberSystem(const SaberSystem &) = delete;
  SaberSystem &operator=(const SaberSystem &) = delete;

  [[nodiscard]] esp_err_t start();

private:
  [[nodiscard]] esp_err_t internalStart();

  // ── Hardware ──
  Hardware::SdHardware m_sdHardware;
  Hardware::AudioHardware m_audioHardware;
  Hardware::LedHardware m_ledHardware;
  Hardware::ImuHardware m_imuHardware;
  Hardware::ButtonHardware m_btnHardware;
  Espressif::Wrappers::RgbLed m_statusLed;

  // ── Adapters ──
  std::unique_ptr<Adapters::ImuAdapter> m_imuAdapter;
  std::unique_ptr<Adapters::InputAdapter> m_inputAdapter;

  // ── Core ──
  Core::SaberActionBus m_bus;
  Profiles::ProfileManager m_profileManager;

#if CONFIG_IDF_TARGET_ESP32S3
  PsramAudioCache m_psramCache;
#endif
};

} // namespace InertialSaber::System
