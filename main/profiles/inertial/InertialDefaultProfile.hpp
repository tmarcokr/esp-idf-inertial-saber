#pragma once

#include "interfaces/InertialProfile.hpp"
#include "profiles/ConfigurableProfile.hpp"

namespace InertialSaber::Profiles {

/**
 * @brief The "inertial" factory profile.
 *
 * Implemented as a wrapper around ConfigurableProfile with static default
 * parameters to preserve identical runtime behavior.
 */
class InertialDefaultProfile final : public Core::InertialProfile {
public:
  InertialDefaultProfile();

  [[nodiscard]] const Core::InertialDefinition &getDefinition() const override;

  void load(Core::SaberActionBus &bus,
            Espressif::Wrappers::Audio::AudioEngine &audio,
            Espressif::Wrappers::SmartLed::Engine &led) override;

  void unload(Core::SaberActionBus &bus) override;

  [[nodiscard]] PowerState getPowerState() const override;

  void setPowerState(PowerState state) override;

private:
  ConfigurableProfile m_impl;
};

} // namespace InertialSaber::Profiles
