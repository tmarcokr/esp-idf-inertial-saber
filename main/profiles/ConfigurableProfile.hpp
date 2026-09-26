#pragma once

#include "profiles/inertial/InertialDefinition.hpp"
#include "profiles/SaberServices.hpp"
#include "profiles/SoundFont.hpp"
#include <cstdint>
#include <memory>
#include <string_view>

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
   * @brief Parses a profile.json document once and builds the profile from it.
   * @return The profile, or nullptr if the JSON cannot be parsed.
   */
  [[nodiscard]] static std::unique_ptr<ConfigurableProfile> fromJson(std::string_view json);

  explicit ConfigurableProfile(Inertial::InertialDefinition definition);

  ConfigurableProfile(const ConfigurableProfile &) = delete;
  ConfigurableProfile &operator=(const ConfigurableProfile &) = delete;
  ConfigurableProfile(ConfigurableProfile &&) = delete;
  ConfigurableProfile &operator=(ConfigurableProfile &&) = delete;

  [[nodiscard]] const Inertial::InertialDefinition &definition() const;
  [[nodiscard]] const SoundFont &soundFont() const;

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
  Inertial::InertialDefinition m_def;
  SoundFont m_font;
  PowerState m_powerState = PowerState::RETRACTED;
};

} // namespace InertialSaber::Profiles
