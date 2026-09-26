#pragma once

#include "profiles/ConfigurableProfile.hpp"
#include "profiles/SaberServices.hpp"
#include <vector>
#include <memory>

namespace InertialSaber::Profiles {

/**
 * @brief Manages the collection of loaded profiles and coordinates runtime hot-swapping.
 */
class ProfileManager {
public:
  explicit ProfileManager(const SaberServices &services) : m_services(services) {}
  ~ProfileManager() = default;

  ProfileManager(const ProfileManager &) = delete;
  ProfileManager &operator=(const ProfileManager &) = delete;

  /**
   * @brief Discovers and initializes profiles from the SD card.
   */
  void init();

  /**
   * @brief Loads the initial active profile onto the bus.
   */
  void loadActive();

  /**
   * @brief Hot-swaps to the next profile in the list.
   */
  void next();

private:
  void saveActiveIndex();

  const SaberServices &m_services;
  std::vector<std::unique_ptr<ConfigurableProfile>> m_profiles;
  size_t m_activeIndex = 0;
};

} // namespace InertialSaber::Profiles
