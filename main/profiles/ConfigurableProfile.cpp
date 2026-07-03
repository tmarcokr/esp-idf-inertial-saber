#include "profiles/ConfigurableProfile.hpp"
#include "profiles/ProfileParser.hpp"
#include "profiles/ProfileManager.hpp"
#include "InertialLightEffect.hpp"
#include "PowerToggleEffect.hpp"
#include "InertialSwingEffect.hpp"
#include "BlasterEffect.hpp"
#include "KineticImpactEffect.hpp"
#include "DragEffect.hpp"
#include "profiles/inertial/effects/ProfileCycleEffect.hpp"
#include "profiles/inertial/effects/PreloadWaitEffect.hpp"
#include "system/hardware/HardwareConfig.hpp"

#include "system/PsramAudioCache.hpp"


#include "esp_log.h"

namespace InertialSaber::Profiles {

static constexpr const char *TAG = "ConfigurableProfile";

ConfigurableProfile::ConfigurableProfile(const InertialSaber::Profiles::Inertial::InertialDefinition &def)
    : m_def(def) {}

ConfigurableProfile::ConfigurableProfile(const std::string &jsonStr)
    : m_def(m_allocatedDef) {
  ProfileParser::parse(jsonStr.c_str(), m_allocatedDef, m_profileNameStorage,
                       m_profileRootStorage);
  m_allocatedDef.profileName = m_profileNameStorage.c_str();
  m_allocatedDef.profileRoot = m_profileRootStorage.c_str();
}

const InertialSaber::Profiles::Inertial::InertialDefinition &ConfigurableProfile::getDefinition() const {
  return m_def;
}

ConfigurableProfile::PowerState ConfigurableProfile::getPowerState() const {
  return m_powerState;
}

void ConfigurableProfile::setPowerState(PowerState state) {
  m_powerState = state;
}

void ConfigurableProfile::load(Core::SaberActionBus &bus,
                               Espressif::Wrappers::Audio::AudioEngine &audio,
                               Espressif::Wrappers::SmartLed::Engine &led,
                               ProfileManager &profileManager,
                               Espressif::Wrappers::RgbLed* statusLed
                               , InertialSaber::System::PsramAudioCache* psramCache

  ) {
  ESP_LOGI(TAG, "Loading configurable profile '%s'", m_def.profileName);

  m_powerState = PowerState::PRELOADING;
  bus.setPhysicsConfig(m_def);

  if (psramCache) {
      psramCache->requestProfilePreload(m_def.profileRoot, m_def.fontSwingPairCount);
  }


  bus.registerEffect(std::make_unique<Effects::PreloadWaitEffect>(
      *this, audio, statusLed
      , psramCache

  ));

  auto swingFx =
      std::make_unique<Effects::InertialSwingEffect>(audio, m_def
                                                     , psramCache

      );
  swingEffect = swingFx.get();
  bus.registerEffect(std::move(swingFx));

  auto lightFx =
      std::make_unique<Effects::InertialLightEffect>(led, m_def);
  lightEffect = lightFx.get();
  bus.registerEffect(std::move(lightFx));

  auto powerFx = std::make_unique<Effects::PowerToggleEffect>(
      *this, *swingEffect, *lightEffect, audio, led, m_def, 0);
  auto &powerRef = *powerFx;
  bus.registerEffect(std::move(powerFx));

  bus.registerEffect(std::make_unique<Effects::BlasterEffect>(
      powerRef, audio, led, m_def, 0));

  bus.registerEffect(std::make_unique<Effects::KineticImpactEffect>(
      powerRef, audio, led, m_def));

  bus.registerEffect(std::make_unique<Effects::DragEffect>(
      powerRef, audio, led, m_def, 0));

  bus.registerEffect(std::make_unique<Effects::ProfileCycleEffect>(
      *this,
      profileManager,
      bus,
      audio,
      led,
      System::Hardware::HardwareConfig::kMainBtnInputId));
}

void ConfigurableProfile::unload(Core::SaberActionBus &bus
                                 , InertialSaber::System::PsramAudioCache* psramCache

  ) {
  ESP_LOGI(TAG, "Unloading configurable profile '%s'", m_def.profileName);

  if (swingEffect) {
    swingEffect->deactivate();
  }
  if (lightEffect) {
    lightEffect->deactivate();
  }

  bus.clearEffects();

  if (psramCache) {
      psramCache->unloadAll();
  }


  swingEffect = nullptr;
  lightEffect = nullptr;
}

} // namespace InertialSaber::Profiles
