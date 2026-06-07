#pragma once

#include "profiles/ConfigurableProfile.hpp"
#include "core/bus/SaberActionBus.hpp"
#include "AudioEngine.hpp"
#include "Engine.hpp"
#include <vector>
#include <memory>

namespace InertialSaber::Profiles {

/**
 * @brief Manages the collection of loaded profiles and coordinates runtime hot-swapping.
 */
class ProfileManager {
public:
  ProfileManager() = default;
  ~ProfileManager() = default;

  ProfileManager(const ProfileManager &) = delete;
  ProfileManager &operator=(const ProfileManager &) = delete;

  /**
   * @brief Discovers and initializes profiles from the SD card.
   *
   * Falls back to a compiled-in default profile if no profiles are discovered.
   */
  void init();

  /**
   * @brief Loads the initial active profile onto the bus.
   */
  void loadActive(Core::SaberActionBus &bus,
                  Espressif::Wrappers::Audio::AudioEngine &audio,
                  Espressif::Wrappers::SmartLed::Engine &led);

  /**
   * @brief Hot-swaps to the next profile in the list.
   */
  void nextProfile(Core::SaberActionBus &bus,
                   Espressif::Wrappers::Audio::AudioEngine &audio,
                   Espressif::Wrappers::SmartLed::Engine &led);

  /**
   * @brief Hot-swaps to the previous profile in the list.
   */
  void prevProfile(Core::SaberActionBus &bus,
                   Espressif::Wrappers::Audio::AudioEngine &audio,
                   Espressif::Wrappers::SmartLed::Engine &led);

  /**
   * @brief Returns the currently active profile.
   */
  [[nodiscard]] ConfigurableProfile &getActiveProfile() const;

  /**
   * @brief Returns the number of loaded profiles.
   */
  [[nodiscard]] size_t getProfileCount() const;

private:
  void saveActiveIndex();

  std::vector<std::unique_ptr<ConfigurableProfile>> m_profiles;
  size_t m_activeIndex = 0;
};

} // namespace InertialSaber::Profiles
