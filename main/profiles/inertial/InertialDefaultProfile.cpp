#include "profiles/inertial/InertialDefaultProfile.hpp"
#include "InertialLightEffect.hpp"
#include "PowerToggleEffect.hpp"
#include "InertialSwingEffect.hpp"
#include "BlasterEffect.hpp"
#include "KineticImpactEffect.hpp"

#include "esp_log.h"

namespace InertialSaber::Profiles {

static constexpr const char *TAG = "InertialProfile";

static const Core::InertialDefinition kDefinition = {
    .profileName = "inertial",
    .profileRoot = "profiles/inertial/",

    .overloadThresholdG = 1.0f,
    .overloadChargeRate = 2.0f,
    .overloadDrainRate = 0.5f,
    .burstCooldownMs = 1500.0f,

    .swingIdleThresholdG = 0.15f,
    .swingMaxThresholdG = 1.0f,
    .swingCrossfadeLowG = 0.4f,
    .swingCrossfadeHighG = 1.0f,
    .gravityInfluence = 0.2f,
    .humBaseVolume = 8000,
    .humMaxDucking = 0.75f,
    .swingSwapCooldownMs = 1000,
    .swingSwapMinVolume = 0.40f,

    .fontHumCount = 1,
    .fontSwingPairCount = 3,
    .fontBurstCount = 16,
    .fontInCount  = 2,
    .fontOutCount = 4,
    .ignitionDurationMs = 800,
    .retractionDurationMs = 500,
    .fontBlasterCount = 8,
    .blasterLedCount = 3,
    .blasterDurationMs = 250,
    .fontClashCount = 16,
    .clashThresholdG = 2.0f,
    .clashDurationMs = 150,

    .bladeBaseHue = 240,
    .lightIdleBaseFreq = 1.0f,
    .lightIdlePulseDepth = 0.15f,
    .lightMaxThermalBleed = 0.80f,
    .lightFlickerIntensity = 0.20f,
    .lightBurstDurationMs = 150,
};


const Core::InertialDefinition &InertialDefaultProfile::getDefinition() const {
  return kDefinition;
}

void InertialDefaultProfile::load(
    Core::SaberActionBus &bus, Espressif::Wrappers::Audio::AudioEngine &audio,
    Espressif::Wrappers::SmartLed::Engine &led) {
  ESP_LOGI(TAG, "Loading profile '%s'", kDefinition.profileName);

  auto swingFx =
      std::make_unique<Effects::InertialSwingEffect>(audio, kDefinition);
  swingEffect = swingFx.get();
  bus.registerEffect(std::move(swingFx));

  auto lightFx =
      std::make_unique<Effects::InertialLightEffect>(led, kDefinition);
  lightEffect = lightFx.get();
  bus.registerEffect(std::move(lightFx));

  auto powerFx = std::make_unique<Effects::PowerToggleEffect>(
      *swingEffect, *lightEffect, audio, led, kDefinition, 0);
  auto &powerRef = *powerFx;
  bus.registerEffect(std::move(powerFx));

  bus.registerEffect(std::make_unique<Effects::BlasterEffect>(
      powerRef, audio, led, kDefinition, 0));

  bus.registerEffect(std::make_unique<Effects::KineticImpactEffect>(
      powerRef, audio, led, kDefinition));

  ESP_LOGI(TAG, "Profile '%s' loaded — 5 effects registered",
           kDefinition.profileName);
}

void InertialDefaultProfile::unload(Core::SaberActionBus &bus) {
  ESP_LOGI(TAG, "Unloading profile '%s'", kDefinition.profileName);

  if (swingEffect)
    swingEffect->deactivate();
  if (lightEffect)
    lightEffect->deactivate();

  bus.clearEffects();

  swingEffect = nullptr;
  lightEffect = nullptr;

  ESP_LOGI(TAG, "Profile '%s' unloaded", kDefinition.profileName);
}

} // namespace InertialSaber::Profiles
