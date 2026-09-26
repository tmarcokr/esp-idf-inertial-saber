#include "profiles/ProfileParser.hpp"
#include "profiles/SoundFont.hpp"
#include "cJSON.h"
#include "esp_log.h"
#include <memory>

namespace InertialSaber::Profiles {

namespace {

float getFloat(const cJSON *parent, const char *key, float defaultValue) {
  if (!parent) return defaultValue;
  cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, key);
  if (cJSON_IsNumber(item)) {
    return static_cast<float>(item->valuedouble);
  }
  return defaultValue;
}

uint8_t getUint8(const cJSON *parent, const char *key, uint8_t defaultValue) {
  if (!parent) return defaultValue;
  cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, key);
  if (cJSON_IsNumber(item)) {
    return static_cast<uint8_t>(item->valueint);
  }
  return defaultValue;
}

uint16_t getUint16(const cJSON *parent, const char *key, uint16_t defaultValue) {
  if (!parent) return defaultValue;
  cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, key);
  if (cJSON_IsNumber(item)) {
    return static_cast<uint16_t>(item->valueint);
  }
  return defaultValue;
}

uint32_t getUint32(const cJSON *parent, const char *key, uint32_t defaultValue) {
  if (!parent) return defaultValue;
  cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, key);
  if (cJSON_IsNumber(item)) {
    return static_cast<uint32_t>(item->valueint);
  }
  return defaultValue;
}

} // namespace

esp_err_t ProfileParser::parse(std::string_view json, Inertial::InertialDefinition &outDef) {
  if (json.empty()) {
    return ESP_ERR_INVALID_ARG;
  }

  std::unique_ptr<cJSON, decltype(&cJSON_Delete)> document(
      cJSON_ParseWithLength(json.data(), json.size()), &cJSON_Delete);
  if (!document) {
    return ESP_FAIL;
  }
  const cJSON *root = document.get();

  cJSON *nameItem = cJSON_GetObjectItemCaseSensitive(root, "name");
  if (cJSON_IsString(nameItem) && nameItem->valuestring) {
    outDef.profileName = nameItem->valuestring;
  } else {
    outDef.profileName = "unnamed";
  }

  cJSON *rootPathItem = cJSON_GetObjectItemCaseSensitive(root, "root_path");
  if (cJSON_IsString(rootPathItem) && rootPathItem->valuestring) {
    outDef.profileRoot = rootPathItem->valuestring;
  } else {
    outDef.profileRoot = "profiles/unnamed/";
  }

  cJSON *overload = cJSON_GetObjectItemCaseSensitive(root, "overload");
  outDef.overloadThresholdG = getFloat(overload, "threshold_g", 1.0f);
  outDef.overloadChargeRate = getFloat(overload, "charge_rate", 2.0f);
  outDef.overloadDrainRate = getFloat(overload, "drain_rate", 0.5f);
  outDef.burstCooldownMs = getFloat(overload, "burst_cooldown_ms", 1500.0f);

  cJSON *sensor = cJSON_GetObjectItemCaseSensitive(root, "sensor");
  outDef.kineticEnergyDeadbandG = getFloat(sensor, "kinetic_deadband_g",    0.25f);
  outDef.rotationDeadbandDps    = getFloat(sensor, "rotation_deadband_dps", 15.0f);

  cJSON *swing = cJSON_GetObjectItemCaseSensitive(root, "swing");
  outDef.swingIdleThresholdG = getFloat(swing, "idle_threshold_g", 0.15f);
  outDef.swingMaxThresholdG = getFloat(swing, "max_threshold_g", 1.0f);
  outDef.swingCrossfadeLowG = getFloat(swing, "crossfade_low_g", 0.4f);
  outDef.swingCrossfadeHighG = getFloat(swing, "crossfade_high_g", 1.0f);
  outDef.gravityInfluence = getFloat(swing, "gravity_influence", 0.2f);
  outDef.humBaseVolume = getUint16(swing, "hum_base_volume", 8000);
  outDef.humMaxDucking = getFloat(swing, "hum_max_ducking", 0.75f);
  outDef.swingSwapCooldownMs = getUint32(swing, "swap_cooldown_ms", 1000);
  outDef.swingSwapMinVolume = getFloat(swing, "swap_min_volume", 0.40f);
  outDef.clashThresholdG = getFloat(swing, "clash_threshold_g", 2.0f);

  cJSON *fontCounts = cJSON_GetObjectItemCaseSensitive(root, "font_counts");
  outDef.fontCounts.hum = getUint8(fontCounts, "hum", 1);
  outDef.fontCounts.swingPair = getUint8(fontCounts, "swing_pair", 3);
  outDef.fontCounts.burst = getUint8(fontCounts, "burst", 16);
  outDef.fontCounts.in = getUint8(fontCounts, "in", 2);
  outDef.fontCounts.out = getUint8(fontCounts, "out", 4);
  outDef.fontCounts.blaster = getUint8(fontCounts, "blaster", 8);
  outDef.fontCounts.clash = getUint8(fontCounts, "clash", 16);
  outDef.fontCounts.drag = getUint8(fontCounts, "drag", 1);
  outDef.fontCounts.dragEnd = getUint8(fontCounts, "drag_end", 4);

  cJSON *bladeTimings = cJSON_GetObjectItemCaseSensitive(root, "blade_timings");
  outDef.ignitionDurationMs = getUint32(bladeTimings, "ignition_duration_ms", 800);
  outDef.retractionDurationMs = getUint32(bladeTimings, "retraction_duration_ms", 500);
  outDef.blasterDurationMs = getUint32(bladeTimings, "blaster_duration_ms", 250);
  outDef.clashDurationMs = getUint32(bladeTimings, "clash_duration_ms", 150);

  cJSON *bladeLeds = cJSON_GetObjectItemCaseSensitive(root, "blade_leds");
  outDef.blasterLedCount = getUint16(bladeLeds, "blaster_count", 3);
  outDef.dragLedCount = getUint16(bladeLeds, "drag_count", 8);

  cJSON *light = cJSON_GetObjectItemCaseSensitive(root, "light");
  outDef.bladeBaseHue = getUint16(light, "blade_base_hue", 240);
  outDef.lightIdleBaseFreq = getFloat(light, "idle_base_freq", 1.0f);
  outDef.lightIdlePulseDepth = getFloat(light, "idle_pulse_depth", 0.15f);
  outDef.lightMaxThermalBleed = getFloat(light, "max_thermal_bleed", 0.80f);
  outDef.lightFlickerIntensity = getFloat(light, "flicker_intensity", 0.20f);
  outDef.lightBurstDurationMs = getUint32(light, "burst_duration_ms", 150);

  return ESP_OK;
}

#ifndef NDEBUG
esp_err_t ProfileParser::runSelfTest() {
  static const char *TAG = "ParserTest";
  ESP_LOGI(TAG, "Running parser self-tests...");

  const char *testJson = R"({
    "name": "test_sith",
    "root_path": "profiles/sith/",
    "sensor": {
      "kinetic_deadband_g": 0.35,
      "rotation_deadband_dps": 18.0
    },
    "overload": {
      "threshold_g": 1.5,
      "charge_rate": 2.5,
      "drain_rate": 0.6,
      "burst_cooldown_ms": 1200.0
    },
    "swing": {
      "idle_threshold_g": 0.2,
      "max_threshold_g": 1.2,
      "crossfade_low_g": 0.5,
      "crossfade_high_g": 1.1,
      "gravity_influence": 0.3,
      "hum_base_volume": 9000,
      "hum_max_ducking": 0.8,
      "swap_cooldown_ms": 1100,
      "swap_min_volume": 0.45,
      "clash_threshold_g": 2.5
    },
    "font_counts": {
      "hum": 2,
      "swing_pair": 4,
      "burst": 12,
      "in": 3,
      "out": 5,
      "blaster": 6,
      "clash": 12,
      "drag": 2,
      "drag_end": 3
    },
    "blade_timings": {
      "ignition_duration_ms": 700,
      "retraction_duration_ms": 400,
      "blaster_duration_ms": 200,
      "clash_duration_ms": 100
    },
    "blade_leds": {
      "blaster_count": 4,
      "drag_count": 6
    },
    "light": {
      "blade_base_hue": 120,
      "idle_base_freq": 1.5,
      "idle_pulse_depth": 0.2,
      "max_thermal_bleed": 0.7,
      "flicker_intensity": 0.3,
      "burst_duration_ms": 120
    }
  })";

  Inertial::InertialDefinition def{};
  if (parse(testJson, def) != ESP_OK) {
    ESP_LOGE(TAG, "Test parse failed");
    return ESP_FAIL;
  }

  if (def.profileName != "test_sith" || def.profileRoot != "profiles/sith/") return ESP_FAIL;
  if (def.kineticEnergyDeadbandG != 0.35f || def.rotationDeadbandDps != 18.0f) return ESP_FAIL;
  if (def.overloadThresholdG != 1.5f || def.overloadChargeRate != 2.5f) return ESP_FAIL;
  if (def.swingIdleThresholdG != 0.2f || def.humBaseVolume != 9000 || def.clashThresholdG != 2.5f) return ESP_FAIL;
  if (def.fontCounts.hum != 2 || def.fontCounts.swingPair != 4 || def.fontCounts.dragEnd != 3) return ESP_FAIL;
  if (def.ignitionDurationMs != 700 || def.retractionDurationMs != 400) return ESP_FAIL;
  if (def.blasterLedCount != 4 || def.dragLedCount != 6) return ESP_FAIL;
  if (def.bladeBaseHue != 120 || def.lightIdleBaseFreq != 1.5f) return ESP_FAIL;

  Inertial::InertialDefinition fallbackDef{};
  if (parse("{}", fallbackDef) != ESP_OK) {
    ESP_LOGE(TAG, "Fallback test parse failed");
    return ESP_FAIL;
  }

  if (fallbackDef.profileName != "unnamed" || fallbackDef.profileRoot != "profiles/unnamed/") return ESP_FAIL;
  if (fallbackDef.overloadThresholdG != 1.0f) return ESP_FAIL;
  if (fallbackDef.kineticEnergyDeadbandG != 0.25f || fallbackDef.rotationDeadbandDps != 15.0f) return ESP_FAIL;
  if (fallbackDef.fontCounts.hum != 1 || fallbackDef.bladeBaseHue != 240 || fallbackDef.clashThresholdG != 2.0f) return ESP_FAIL;

  Inertial::InertialDefinition unslashedDef{};
  if (parse(R"({"root_path": "/profiles/sith"})", unslashedDef) != ESP_OK) {
    ESP_LOGE(TAG, "Unslashed root test parse failed");
    return ESP_FAIL;
  }
  const SoundFont unslashedFont(unslashedDef.profileRoot, unslashedDef.fontCounts);
  if (unslashedFont.root() != "profiles/sith/") return ESP_FAIL;
  if (unslashedFont.pathFor(FontCategory::Blaster, 1) != "/sdcard/profiles/sith/blst/blst1.wav") return ESP_FAIL;
  if (unslashedFont.selectionPath() != "/sdcard/profiles/sith/font.wav") return ESP_FAIL;

  if (SoundFont::normalizeRoot("a") != "a/" || SoundFont::normalizeRoot("a/") != "a/") return ESP_FAIL;
  if (SoundFont::normalizeRoot("/a/") != "a/" || SoundFont::normalizeRoot("a//") != "a/") return ESP_FAIL;
  if (!SoundFont::normalizeRoot("").empty()) return ESP_FAIL;

  ESP_LOGI(TAG, "All parser self-tests passed successfully!");
  return ESP_OK;
}
#endif

} // namespace InertialSaber::Profiles
