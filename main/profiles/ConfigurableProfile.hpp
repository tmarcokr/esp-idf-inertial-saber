#pragma once

#include "interfaces/InertialProfile.hpp"
#include <string>

namespace InertialSaber::Profiles {

/**
 * @brief A generic profile driven by an InertialDefinition structure.
 *
 * Instantiates and registers the core 6 effects suite on the SaberActionBus.
 */
class ConfigurableProfile final : public Core::InertialProfile {
public:
  /**
   * @brief Constructor that binds this profile to an external configuration definition.
   */
  explicit ConfigurableProfile(const Core::InertialDefinition &def);

  /**
   * @brief Constructor that parses a JSON configuration string.
   */
  explicit ConfigurableProfile(const std::string &jsonStr);

  [[nodiscard]] const Core::InertialDefinition &getDefinition() const override;

  void load(Core::SaberActionBus &bus,
            Espressif::Wrappers::Audio::AudioEngine &audio,
            Espressif::Wrappers::SmartLed::Engine &led) override;

  void unload(Core::SaberActionBus &bus) override;

private:
  std::string m_profileNameStorage;
  std::string m_profileRootStorage;
  Core::InertialDefinition m_allocatedDef{};
  const Core::InertialDefinition &m_def;
};

} // namespace InertialSaber::Profiles
