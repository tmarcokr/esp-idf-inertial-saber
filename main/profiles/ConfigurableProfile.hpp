#pragma once

#include "profiles/inertial/InertialDefinition.hpp"
#include "profiles/SaberServices.hpp"
#include <cstdint>
#include <string>

namespace InertialSaber::Effects {
    class InertialSwingEffect;
    class InertialLightEffect;
} // namespace InertialSaber::Effects

namespace InertialSaber::Profiles {

class ProfileManager;

/**
 * @brief A generic profile driven by an InertialDefinition structure.
 *
 * Instantiates and registers the core effects suite on the SaberActionBus.
 * Owns the PowerState lifecycle and non-owning pointers to the engine effects.
 */
class ConfigurableProfile final {
public:
  enum class PowerState : uint8_t {
      RETRACTED,
      IGNITING,
      IGNITED,
      RETRACTING,
      PRELOADING
  };

  /**
   * @brief Constructor that binds this profile to an external configuration definition.
   */
  explicit ConfigurableProfile(const InertialSaber::Profiles::Inertial::InertialDefinition &def);

  /**
   * @brief Constructor that parses a JSON configuration string.
   */
  explicit ConfigurableProfile(const std::string &jsonStr);

  [[nodiscard]] const InertialSaber::Profiles::Inertial::InertialDefinition &definition() const;

  [[nodiscard]] PowerState getPowerState() const;
  void setPowerState(PowerState state);

  /**
   * @brief Instantiate and register this profile's effects on the bus.
   */
  void load(const SaberServices &services, ProfileManager &profileManager);

  /**
   * @brief Deactivate this profile's effects and clear them from the bus.
   */
  void unload(const SaberServices &services);

private:
  Effects::InertialSwingEffect *m_swingEffect = nullptr;
  Effects::InertialLightEffect *m_lightEffect = nullptr;
  std::string m_profileNameStorage;
  std::string m_profileRootStorage;
  InertialSaber::Profiles::Inertial::InertialDefinition m_allocatedDef{};
  const InertialSaber::Profiles::Inertial::InertialDefinition &m_def;
  PowerState m_powerState = PowerState::RETRACTED;
};

} // namespace InertialSaber::Profiles
