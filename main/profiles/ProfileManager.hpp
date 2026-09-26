#pragma once

#include "profiles/ConfigurableProfile.hpp"
#include "profiles/SaberServices.hpp"
#include "esp_err.h"
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
   * @return ESP_OK if at least one valid profile was loaded, otherwise the scan error or ESP_ERR_NOT_FOUND.
   */
  [[nodiscard]] esp_err_t init();

  /**
   * @brief Loads the initial active profile onto the bus.
   * @return ESP_OK, or ESP_ERR_INVALID_STATE if no profile is available.
   */
  [[nodiscard]] esp_err_t loadActive();

  /**
   * @brief Hot-swaps to the next profile in the list; with a single profile, reloads it and re-runs its preload.
   */
  void next();

private:
  void saveActiveIndex();

  const SaberServices &m_services;
  std::vector<std::unique_ptr<ConfigurableProfile>> m_profiles;
  size_t m_activeIndex = 0;
};

} // namespace InertialSaber::Profiles
