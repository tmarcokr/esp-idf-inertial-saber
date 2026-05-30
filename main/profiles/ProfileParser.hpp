#pragma once

#include "core/models/InertialDefinition.hpp"
#include "esp_err.h"
#include <string>

namespace InertialSaber::Profiles {

/**
 * @brief Helper utility to parse JSON configurations into InertialDefinition.
 */
class ProfileParser {
public:
  /**
   * @brief Parses JSON string and populates definition and string storage.
   * @param jsonStr Raw JSON configuration text.
   * @param outDef Definition structure to populate.
   * @param outName Output string for profileName to maintain lifetime.
   * @param outRoot Output string for profileRoot to maintain lifetime.
   * @return ESP_OK on success, or an error code.
   */
  static esp_err_t parse(const char *jsonStr, Core::InertialDefinition &outDef,
                         std::string &outName, std::string &outRoot);

#ifndef NDEBUG
  /**
   * @brief Executes a comprehensive parser self-test checking values and fallbacks.
   * @return ESP_OK on success, or ESP_FAIL if any validation fails.
   */
  static esp_err_t runSelfTest();
#endif
};

} // namespace InertialSaber::Profiles
