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

std::unique_ptr<ConfigurableProfile> ConfigurableProfile::fromJson(std::string_view json) {
  Inertial::InertialDefinition definition{};
  if (ProfileParser::parse(json, definition) != ESP_OK) {
    return nullptr;
  }
  return std::make_unique<ConfigurableProfile>(std::move(definition));
}

ConfigurableProfile::ConfigurableProfile(Inertial::InertialDefinition definition)
    : m_def(std::move(definition)), m_font(m_def.profileRoot, m_def.fontCounts) {}

const Inertial::InertialDefinition &ConfigurableProfile::definition() const {
  return m_def;
}

const SoundFont &ConfigurableProfile::soundFont() const {
  return m_font;
}

ConfigurableProfile::PowerState ConfigurableProfile::getPowerState() const {
  return m_powerState;
}

void ConfigurableProfile::setPowerState(PowerState state) {
  m_powerState = state;
}

void ConfigurableProfile::load(const SaberServices &services, ProfileManager &profileManager) {
  ESP_LOGI(TAG, "Loading configurable profile '%s'", m_def.profileName.c_str());
  logFontPaths();

  m_powerState = PowerState::PRELOADING;
  services.bus.setPhysicsConfig(m_def);

  services.audioCache.requestProfilePreload(m_font.root(), m_font.swingPairCount());

  services.bus.registerEffect(std::make_unique<Effects::PreloadWaitEffect>(
      *this, services.audio, services.audioCache, services.status, m_font));

  auto swingFx = std::make_unique<Effects::InertialSwingEffect>(services.audio, m_def, m_font,
                                                                services.audioCache);
  m_swingEffect = swingFx.get();
  services.bus.registerEffect(std::move(swingFx));

  auto lightFx = std::make_unique<Effects::InertialLightEffect>(services.blade, m_def);
  m_lightEffect = lightFx.get();
  services.bus.registerEffect(std::move(lightFx));

  auto powerFx = std::make_unique<Effects::PowerToggleEffect>(
      *this, *m_swingEffect, *m_lightEffect, services.audio, services.blade, m_def, m_font,
      Core::kMainButtonInputId);
  auto &powerRef = *powerFx;
  services.bus.registerEffect(std::move(powerFx));

  services.bus.registerEffect(std::make_unique<Effects::BlasterEffect>(
      powerRef, services.audio, services.blade, m_def, m_font, Core::kMainButtonInputId));

  services.bus.registerEffect(std::make_unique<Effects::KineticImpactEffect>(
      powerRef, services.audio, services.blade, m_def, m_font));

  services.bus.registerEffect(std::make_unique<Effects::DragEffect>(
      powerRef, services.audio, services.blade, m_def, m_font, Core::kMainButtonInputId));

  services.bus.registerEffect(std::make_unique<Effects::ProfileCycleEffect>(
      *this, profileManager, Core::kMainButtonInputId));
}

void ConfigurableProfile::logFontPaths() const {
  ESP_LOGD(TAG, "Font paths: %s | %s | %s | %s", m_font.humPath().c_str(),
           m_font.selectionPath().c_str(), m_font.swingLowPath(1).c_str(),
           m_font.swingHighPath(1).c_str());
  for (const auto category : {FontCategory::Ignition, FontCategory::Retraction, FontCategory::Blaster,
                              FontCategory::Clash, FontCategory::Drag, FontCategory::DragEnd,
                              FontCategory::Burst}) {
    ESP_LOGD(TAG, "Font paths: %s (count=%u)", m_font.pathFor(category, 1).c_str(),
             m_font.count(category));
  }
}

void ConfigurableProfile::unload(const SaberServices &services) {
  ESP_LOGI(TAG, "Unloading configurable profile '%s'", m_def.profileName.c_str());

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
