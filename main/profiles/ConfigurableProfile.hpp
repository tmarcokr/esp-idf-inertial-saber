#pragma once

#include "profiles/inertial/InertialDefinition.hpp"
#include "core/SaberActionBus.hpp"
#include "AudioEngine.hpp"
#include "Engine.hpp"
#include <cstdint>
#include <string>

namespace InertialSaber::Effects {
    class InertialSwingEffect;
    class InertialLightEffect;
} // namespace InertialSaber::Effects

#if CONFIG_IDF_TARGET_ESP32S3
namespace InertialSaber::System { class PsramAudioCache; }
#endif

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
  explicit ConfigurableProfile(const InertialSaber::Profiles::Inertial::InertialDefinition &def);

  /**
   * @brief Constructor that parses a JSON configuration string.
   */
  explicit ConfigurableProfile(const std::string &jsonStr);

  [[nodiscard]] const InertialSaber::Profiles::Inertial::InertialDefinition &getDefinition() const;

  [[nodiscard]] PowerState getPowerState() const;
  void setPowerState(PowerState state);

  /**
   * @brief Instantiate and register this profile's effects on the bus.
   */
  void load(Core::SaberActionBus &bus,
            Espressif::Wrappers::Audio::AudioEngine &audio,
            Espressif::Wrappers::SmartLed::Engine &led,
            ProfileManager &profileManager
#if CONFIG_IDF_TARGET_ESP32S3
            , InertialSaber::System::PsramAudioCache* psramCache = nullptr
#endif
  );

  /**
   * @brief Deactivate this profile's effects and clear them from the bus.
   */
  void unload(Core::SaberActionBus &bus
#if CONFIG_IDF_TARGET_ESP32S3
              , InertialSaber::System::PsramAudioCache* psramCache = nullptr
#endif
  );

  Effects::InertialSwingEffect* swingEffect = nullptr;
  Effects::InertialLightEffect* lightEffect = nullptr;

private:
  std::string m_profileNameStorage;
  std::string m_profileRootStorage;
  InertialSaber::Profiles::Inertial::InertialDefinition m_allocatedDef{};
  const InertialSaber::Profiles::Inertial::InertialDefinition &m_def;
  PowerState m_powerState = PowerState::RETRACTED;
};

} // namespace InertialSaber::Profiles
