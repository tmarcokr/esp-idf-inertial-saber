#include "profiles/inertial/InertialDefaultProfile.hpp"
#include "InertialLightEffect.hpp"
#include "PowerToggleEffect.hpp"
#include "inertial_swing/InertialSwingEffect.hpp"

#include "esp_log.h"

namespace InertialSaber::Profiles {

static constexpr const char *TAG = "InertialProfile";

// ── Static definition
// ────────────────────────────────────────────────────────── All values mirror
// the previous PlatformConfig.hpp per-profile constants. This is the single
// authoritative source for the "inertial" profile tuning.
static const Core::InertialDefinition kDefinition = {
    // Identity
    .profileName = "inertial",
    .profileRoot = "profiles/inertial/",

    // Inertial Overload
    .overloadThresholdG = 1.0f,
    .overloadChargeRate = 2.0f,
    .overloadDrainRate = 0.5f,
    .burstCooldownMs = 1500.0f,

    // InertialSwing Audio
    .swingIdleThresholdG = 0.15f,
    .swingMaxThresholdG = 1.0f,
    .swingCrossfadeLowG = 0.4f,
    .swingCrossfadeHighG = 1.0f,
    .gravityInfluence = 0.2f,
    .humBaseVolume = 8000,
    .humMaxDucking = 0.75f,
    .swingSwapCooldownMs = 1000,
    .swingSwapMinVolume = 0.40f,

    // Font file counts
    .fontHumCount = 1,
    .fontSwingPairCount = 3,
    .fontBurstCount = 16,

    // InertialLight Visual
    .bladeBaseHue = 240, // Blue in HSB (0–359)
    .lightIdleBaseFreq = 1.0f,
    .lightIdlePulseDepth = 0.15f,
    .lightMaxThermalBleed = 0.80f,
    .lightFlickerIntensity = 0.20f,
    .lightBurstDurationMs = 150,
};

// ── InertialProfile contract
// ───────────────────────────────────────────────────

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

  bus.registerEffect(
      std::make_unique<Effects::PowerToggleEffect>(*swingEffect, *lightEffect,
                                                   /*buttonId=*/0));

  ESP_LOGI(TAG, "Profile '%s' loaded — 3 effects registered",
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
