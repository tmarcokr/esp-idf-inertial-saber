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
#include "core/SaberDataPacket.hpp"

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

const InertialSaber::Profiles::Inertial::InertialDefinition &ConfigurableProfile::definition() const {
  return m_def;
}

ConfigurableProfile::PowerState ConfigurableProfile::getPowerState() const {
  return m_powerState;
}

void ConfigurableProfile::setPowerState(PowerState state) {
  m_powerState = state;
}

void ConfigurableProfile::load(const SaberServices &services, ProfileManager &profileManager) {
  ESP_LOGI(TAG, "Loading configurable profile '%s'", m_def.profileName);

  m_powerState = PowerState::PRELOADING;
  services.bus.setPhysicsConfig(m_def);

  services.audioCache.requestProfilePreload(m_def.profileRoot, m_def.fontSwingPairCount);

  services.bus.registerEffect(std::make_unique<Effects::PreloadWaitEffect>(
      *this, services.audio, services.audioCache, services.status));

  auto swingFx = std::make_unique<Effects::InertialSwingEffect>(services.audio, m_def,
                                                                services.audioCache);
  m_swingEffect = swingFx.get();
  services.bus.registerEffect(std::move(swingFx));

  auto lightFx = std::make_unique<Effects::InertialLightEffect>(services.blade, m_def);
  m_lightEffect = lightFx.get();
  services.bus.registerEffect(std::move(lightFx));

  auto powerFx = std::make_unique<Effects::PowerToggleEffect>(
      *this, *m_swingEffect, *m_lightEffect, services.audio, services.blade, m_def,
      Core::kMainButtonInputId);
  auto &powerRef = *powerFx;
  services.bus.registerEffect(std::move(powerFx));

  services.bus.registerEffect(std::make_unique<Effects::BlasterEffect>(
      powerRef, services.audio, services.blade, m_def, Core::kMainButtonInputId));

  services.bus.registerEffect(std::make_unique<Effects::KineticImpactEffect>(
      powerRef, services.audio, services.blade, m_def));

  services.bus.registerEffect(std::make_unique<Effects::DragEffect>(
      powerRef, services.audio, services.blade, m_def, Core::kMainButtonInputId));

  services.bus.registerEffect(std::make_unique<Effects::ProfileCycleEffect>(
      *this, profileManager, Core::kMainButtonInputId));
}

void ConfigurableProfile::unload(const SaberServices &services) {
  ESP_LOGI(TAG, "Unloading configurable profile '%s'", m_def.profileName);

  if (m_swingEffect) {
    m_swingEffect->deactivate();
  }
  if (m_lightEffect) {
    m_lightEffect->deactivate();
  }

  services.bus.clearEffects();

  services.audioCache.unloadAll();

  m_swingEffect = nullptr;
  m_lightEffect = nullptr;
}

} // namespace InertialSaber::Profiles
