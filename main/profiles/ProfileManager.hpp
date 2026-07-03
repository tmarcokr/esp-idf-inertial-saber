#pragma once

#include "profiles/ConfigurableProfile.hpp"
#include "core/SaberActionBus.hpp"
#include "AudioEngine.hpp"
#include "Engine.hpp"
#include <vector>
#include <memory>

namespace Espressif::Wrappers { class RgbLed; }

namespace InertialSaber::System { class PsramAudioCache; }


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

  void setStatusLed(Espressif::Wrappers::RgbLed* statusLed) { m_statusLed = statusLed; }

  void setPsramCache(InertialSaber::System::PsramAudioCache* psramCache) { m_psramCache = psramCache; }


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
  Espressif::Wrappers::RgbLed* m_statusLed = nullptr;

  InertialSaber::System::PsramAudioCache* m_psramCache = nullptr;

};

} // namespace InertialSaber::Profiles
