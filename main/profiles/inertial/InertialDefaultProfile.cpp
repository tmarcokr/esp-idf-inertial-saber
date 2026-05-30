#include "profiles/inertial/InertialDefaultProfile.hpp"
#include "InertialLightEffect.hpp"
#include "PowerToggleEffect.hpp"
#include "InertialSwingEffect.hpp"
#include "BlasterEffect.hpp"
#include "KineticImpactEffect.hpp"
#include "DragEffect.hpp"

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

    .fontDragCount = 1,
    .fontDragEndCount = 4,
    .dragLedCount = 8,

    .bladeBaseHue = 240,
    .lightIdleBaseFreq = 1.0f,
    .lightIdlePulseDepth = 0.15f,
    .lightMaxThermalBleed = 0.80f,
    .lightFlickerIntensity = 0.20f,
    .lightBurstDurationMs = 150,
};


InertialDefaultProfile::InertialDefaultProfile() : m_impl(kDefinition) {}

const Core::InertialDefinition &
InertialDefaultProfile::getDefinition() const {
  return m_impl.getDefinition();
}

void InertialDefaultProfile::load(Core::SaberActionBus &bus,
                                 Espressif::Wrappers::Audio::AudioEngine &audio,
                                 Espressif::Wrappers::SmartLed::Engine &led) {
  m_impl.load(bus, audio, led);
  swingEffect = m_impl.swingEffect;
  lightEffect = m_impl.lightEffect;
}

void InertialDefaultProfile::unload(Core::SaberActionBus &bus) {
  m_impl.unload(bus);
  swingEffect = nullptr;
  lightEffect = nullptr;
}

Core::InertialProfile::PowerState InertialDefaultProfile::getPowerState() const {
  return m_impl.getPowerState();
}

void InertialDefaultProfile::setPowerState(PowerState state) {
  m_impl.setPowerState(state);
}

} // namespace InertialSaber::Profiles
