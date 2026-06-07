#pragma once

#include "profiles/ConfigurableProfile.hpp"
#include "esp_err.h"
#include <memory>
#include <vector>

namespace InertialSaber::Profiles {

/**
 * @brief Helper utility to scan the SD card filesystem and load profiles dynamically.
 */
class ProfileLoader {
public:
  /**
   * @brief Scans /sdcard/profiles/ and appends successfully parsed profiles to the vector.
   * @param profiles Vector to append the loaded profile pointers to.
   * @return ESP_OK on success, or an error code.
   */
  static esp_err_t loadFromSd(std::vector<std::unique_ptr<ConfigurableProfile>> &profiles);
};

} // namespace InertialSaber::Profiles
