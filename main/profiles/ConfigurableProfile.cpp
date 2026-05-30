#include "profiles/ConfigurableProfile.hpp"
#include "profiles/ProfileParser.hpp"
#include "InertialLightEffect.hpp"
#include "PowerToggleEffect.hpp"
#include "InertialSwingEffect.hpp"
#include "BlasterEffect.hpp"
#include "KineticImpactEffect.hpp"
#include "DragEffect.hpp"

#include "esp_log.h"

namespace InertialSaber::Profiles {

static constexpr const char *TAG = "ConfigurableProfile";

ConfigurableProfile::ConfigurableProfile(const Core::InertialDefinition &def)
    : m_def(def) {}

ConfigurableProfile::ConfigurableProfile(const std::string &jsonStr)
    : m_def(m_allocatedDef) {
  ProfileParser::parse(jsonStr.c_str(), m_allocatedDef, m_profileNameStorage,
                       m_profileRootStorage);
  m_allocatedDef.profileName = m_profileNameStorage.c_str();
  m_allocatedDef.profileRoot = m_profileRootStorage.c_str();
}

const Core::InertialDefinition &ConfigurableProfile::getDefinition() const {
  return m_def;
}

void ConfigurableProfile::load(Core::SaberActionBus &bus,
                               Espressif::Wrappers::Audio::AudioEngine &audio,
                               Espressif::Wrappers::SmartLed::Engine &led) {
  ESP_LOGI(TAG, "Loading configurable profile '%s'", m_def.profileName);

  auto swingFx =
      std::make_unique<Effects::InertialSwingEffect>(audio, m_def);
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
}

void ConfigurableProfile::unload(Core::SaberActionBus &bus) {
  ESP_LOGI(TAG, "Unloading configurable profile '%s'", m_def.profileName);

  if (swingEffect) {
    swingEffect->deactivate();
  }
  if (lightEffect) {
    lightEffect->deactivate();
  }

  bus.clearEffects();

  swingEffect = nullptr;
  lightEffect = nullptr;
}

} // namespace InertialSaber::Profiles
