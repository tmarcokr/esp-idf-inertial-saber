#pragma once

#include "core/models/InertialDefinition.hpp"
#include "core/bus/SaberActionBus.hpp"
#include "AudioEngine.hpp"
#include "Engine.hpp"
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
      RETRACTING
  };

  /**
   * @brief Constructor that binds this profile to an external configuration definition.
   */
  explicit ConfigurableProfile(const Core::InertialDefinition &def);

  /**
   * @brief Constructor that parses a JSON configuration string.
   */
  explicit ConfigurableProfile(const std::string &jsonStr);

  [[nodiscard]] const Core::InertialDefinition &getDefinition() const;

  [[nodiscard]] PowerState getPowerState() const;
  void setPowerState(PowerState state);

  /**
   * @brief Instantiate and register this profile's effects on the bus.
   */
  void load(Core::SaberActionBus &bus,
            Espressif::Wrappers::Audio::AudioEngine &audio,
            Espressif::Wrappers::SmartLed::Engine &led,
            ProfileManager &profileManager);

  /**
   * @brief Deactivate this profile's effects and clear them from the bus.
   */
  void unload(Core::SaberActionBus &bus);

  Effects::InertialSwingEffect* swingEffect = nullptr;
  Effects::InertialLightEffect* lightEffect = nullptr;

private:
  std::string m_profileNameStorage;
  std::string m_profileRootStorage;
  Core::InertialDefinition m_allocatedDef{};
  const Core::InertialDefinition &m_def;
  PowerState m_powerState = PowerState::RETRACTED;
};

} // namespace InertialSaber::Profiles
