#pragma once

#include "profiles/inertial/InertialDefinition.hpp"
#include "esp_err.h"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace InertialSaber::Profiles {

/**
 * @brief Helper utility to parse JSON configurations into InertialDefinition.
 */
class ProfileParser {
public:
  /**
   * @brief Parses a profile.json document into a definition, applying defaults for missing keys.
   *
   * Out-of-range or non-numeric values are replaced (clamped or defaulted) with a warning per field.
   * @param json Raw JSON configuration text (no null terminator required).
   * @param outDef Definition structure to populate.
   * @return ESP_OK on success, ESP_ERR_INVALID_ARG for empty input, ESP_FAIL on malformed JSON.
   */
  static esp_err_t parse(std::string_view json, Inertial::InertialDefinition &outDef);

#ifndef NDEBUG
  /**
   * @brief Executes a comprehensive parser self-test checking values and fallbacks.
   * @return ESP_OK on success, or ESP_FAIL if any validation fails.
   */
  static esp_err_t runSelfTest();
#endif

private:
  enum class Diagnostics : uint8_t { Log, Silent };

  static esp_err_t parse(std::string_view json, Inertial::InertialDefinition &outDef,
                         Diagnostics diagnostics, size_t &corrections);
};

} // namespace InertialSaber::Profiles
