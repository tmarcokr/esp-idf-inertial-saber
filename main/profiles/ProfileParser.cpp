#include "profiles/ProfileParser.hpp"
#include "profiles/SoundFont.hpp"
#include "profiles/inertial/effects/AudioLevels.hpp"
#include "InertialLightEffect.hpp"
#include "system/PsramAudioCache.hpp"
#include "cJSON.h"
#include "esp_log.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <type_traits>

namespace InertialSaber::Profiles {

namespace {

constexpr const char *TAG = "ProfileParser";

constexpr float kMaxG = 16.0f;
constexpr float kMinGSpan = 0.05f;
constexpr float kMaxSensorDeadbandG = 2.0f;
constexpr float kMaxRotationDeadbandDps = 2000.0f;
constexpr float kMaxOverloadRatePerS = 100.0f;
constexpr float kMinClashThresholdG = 0.1f;
constexpr float kMaxCooldownMs = 60000.0f;
constexpr uint32_t kMaxSwapCooldownMs = 60000;
constexpr uint32_t kMinDurationMs = 1;
constexpr uint32_t kMaxDurationMs = 30000;
constexpr uint16_t kMinLedCount = 1;
constexpr uint16_t kMaxHue = 359;
constexpr float kMinIdleBaseFreqHz = Effects::InertialLightEffect::kGravityBreathModulationHz;
constexpr float kMaxIdleBaseFreqHz = 10.0f;
constexpr uint8_t kMaxFontCount = std::numeric_limits<uint8_t>::max();

struct Section {
  const cJSON *node;
  const char *name;
};

Section section(const cJSON *root, const char *name) {
  return {cJSON_GetObjectItemCaseSensitive(root, name), name};
}

class FieldReader {
public:
  FieldReader(const char *profileName, bool logCorrections)
      : m_profileName(profileName), m_logCorrections(logCorrections) {}

  template <typename T>
  T read(const Section &parent, const char *key, T defaultValue, T minValue, T maxValue) {
    if (!parent.node) return defaultValue;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent.node, key);
    if (!item) return defaultValue;

    if (!cJSON_IsNumber(item) || std::isnan(item->valuedouble)) {
      ++m_corrections;
      if (m_logCorrections) {
        ESP_LOGW(TAG, "'%s': %s.%s is not a number, using default %g", m_profileName, parent.name,
                 key, static_cast<double>(defaultValue));
      }
      return defaultValue;
    }

    double raw = item->valuedouble;
    if constexpr (std::is_integral_v<T>) {
      raw = std::trunc(raw);
    } else {
      constexpr auto kLowest = static_cast<double>(std::numeric_limits<T>::lowest());
      constexpr auto kHighest = static_cast<double>(std::numeric_limits<T>::max());
      raw = static_cast<double>(static_cast<T>(std::clamp(raw, kLowest, kHighest)));
    }
    const double clamped =
        std::clamp(raw, static_cast<double>(minValue), static_cast<double>(maxValue));
    if (clamped != raw) {
      ++m_corrections;
      if (m_logCorrections) {
        ESP_LOGW(TAG, "'%s': %s.%s = %g out of range [%g, %g], clamped to %g", m_profileName,
                 parent.name, key, raw, static_cast<double>(minValue),
                 static_cast<double>(maxValue), clamped);
      }
    }
    return static_cast<T>(clamped);
  }

  void enforceSpan(const Section &parent, const char *lowKey, float low, const char *highKey,
                   float &high) {
    if (high > low) return;
    const float corrected = low + kMinGSpan;
    ++m_corrections;
    if (m_logCorrections) {
      ESP_LOGW(TAG, "'%s': %s.%s = %g must exceed %s = %g, raised to %g", m_profileName,
               parent.name, highKey, static_cast<double>(high), lowKey, static_cast<double>(low),
               static_cast<double>(corrected));
    }
    high = corrected;
  }

  [[nodiscard]] size_t corrections() const { return m_corrections; }

private:
  const char *m_profileName;
  bool m_logCorrections;
  size_t m_corrections = 0;
};

} // namespace

esp_err_t ProfileParser::parse(std::string_view json, Inertial::InertialDefinition &outDef) {
  size_t corrections = 0;
  return parse(json, outDef, Diagnostics::Log, corrections);
}

esp_err_t ProfileParser::parse(std::string_view json, Inertial::InertialDefinition &outDef,
                               Diagnostics diagnostics, size_t &corrections) {
  corrections = 0;
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

  FieldReader reader(outDef.profileName.c_str(), diagnostics == Diagnostics::Log);

  const Section overload = section(root, "overload");
  outDef.overloadThresholdG = reader.read(overload, "threshold_g", 1.0f, 0.0f, kMaxG);
  outDef.overloadChargeRate = reader.read(overload, "charge_rate", 2.0f, 0.0f, kMaxOverloadRatePerS);
  outDef.overloadDrainRate = reader.read(overload, "drain_rate", 0.5f, 0.0f, kMaxOverloadRatePerS);
  outDef.burstCooldownMs = reader.read(overload, "burst_cooldown_ms", 1500.0f, 0.0f, kMaxCooldownMs);

  const Section sensor = section(root, "sensor");
  outDef.kineticEnergyDeadbandG = reader.read(sensor, "kinetic_deadband_g",    0.25f, 0.0f, kMaxSensorDeadbandG);
  outDef.rotationDeadbandDps    = reader.read(sensor, "rotation_deadband_dps", 15.0f, 0.0f, kMaxRotationDeadbandDps);

  const Section swing = section(root, "swing");
  outDef.swingIdleThresholdG = reader.read(swing, "idle_threshold_g", 0.15f, 0.0f, kMaxG - kMinGSpan);
  outDef.swingMaxThresholdG = reader.read(swing, "max_threshold_g", 1.0f, 0.0f, kMaxG);
  reader.enforceSpan(swing, "idle_threshold_g", outDef.swingIdleThresholdG, "max_threshold_g",
                     outDef.swingMaxThresholdG);
  outDef.swingCrossfadeLowG = reader.read(swing, "crossfade_low_g", 0.4f, 0.0f, kMaxG - kMinGSpan);
  outDef.swingCrossfadeHighG = reader.read(swing, "crossfade_high_g", 1.0f, 0.0f, kMaxG);
  reader.enforceSpan(swing, "crossfade_low_g", outDef.swingCrossfadeLowG, "crossfade_high_g",
                     outDef.swingCrossfadeHighG);
  outDef.gravityInfluence = reader.read(swing, "gravity_influence", 0.2f, 0.0f, 1.0f);
  outDef.humBaseVolume = reader.read<uint16_t>(swing, "hum_base_volume", 8000, 0, Effects::kFullVolume);
  outDef.humMaxDucking = reader.read(swing, "hum_max_ducking", 0.75f, 0.0f, 1.0f);
  outDef.swingSwapCooldownMs = reader.read<uint32_t>(swing, "swap_cooldown_ms", 1000, 0, kMaxSwapCooldownMs);
  outDef.swingSwapMinVolume = reader.read(swing, "swap_min_volume", 0.40f, 0.0f, 1.0f);
  outDef.clashThresholdG = reader.read(swing, "clash_threshold_g", 2.0f, kMinClashThresholdG, kMaxG);

  const Section fontCounts = section(root, "font_counts");
  outDef.fontCounts.hum = reader.read<uint8_t>(fontCounts, "hum", 1, 0, kMaxFontCount);
  outDef.fontCounts.swingPair = reader.read<uint8_t>(fontCounts, "swing_pair", 3, 0, System::PsramAudioCache::kMaxSwingPairs);
  outDef.fontCounts.burst = reader.read<uint8_t>(fontCounts, "burst", 16, 0, kMaxFontCount);
  outDef.fontCounts.in = reader.read<uint8_t>(fontCounts, "in", 2, 0, kMaxFontCount);
  outDef.fontCounts.out = reader.read<uint8_t>(fontCounts, "out", 4, 0, kMaxFontCount);
  outDef.fontCounts.blaster = reader.read<uint8_t>(fontCounts, "blaster", 8, 0, kMaxFontCount);
  outDef.fontCounts.clash = reader.read<uint8_t>(fontCounts, "clash", 16, 0, kMaxFontCount);
  outDef.fontCounts.drag = reader.read<uint8_t>(fontCounts, "drag", 1, 0, kMaxFontCount);
  outDef.fontCounts.dragEnd = reader.read<uint8_t>(fontCounts, "drag_end", 4, 0, kMaxFontCount);

  const Section bladeTimings = section(root, "blade_timings");
  outDef.ignitionDurationMs = reader.read<uint32_t>(bladeTimings, "ignition_duration_ms", 800, kMinDurationMs, kMaxDurationMs);
  outDef.retractionDurationMs = reader.read<uint32_t>(bladeTimings, "retraction_duration_ms", 500, kMinDurationMs, kMaxDurationMs);
  outDef.blasterDurationMs = reader.read<uint32_t>(bladeTimings, "blaster_duration_ms", 250, kMinDurationMs, kMaxDurationMs);
  outDef.clashDurationMs = reader.read<uint32_t>(bladeTimings, "clash_duration_ms", 150, kMinDurationMs, kMaxDurationMs);

  const Section bladeLeds = section(root, "blade_leds");
  outDef.blasterLedCount = reader.read<uint16_t>(bladeLeds, "blaster_count", 3, kMinLedCount, std::numeric_limits<uint16_t>::max());
  outDef.dragLedCount = reader.read<uint16_t>(bladeLeds, "drag_count", 8, kMinLedCount, std::numeric_limits<uint16_t>::max());

  const Section light = section(root, "light");
  outDef.bladeBaseHue = reader.read<uint16_t>(light, "blade_base_hue", 240, 0, kMaxHue);
  outDef.lightIdleBaseFreq = reader.read(light, "idle_base_freq", 1.0f, kMinIdleBaseFreqHz, kMaxIdleBaseFreqHz);
  outDef.lightIdlePulseDepth = reader.read(light, "idle_pulse_depth", 0.15f, 0.0f, 1.0f);
  outDef.lightMaxThermalBleed = reader.read(light, "max_thermal_bleed", 0.80f, 0.0f, 1.0f);
  outDef.lightFlickerIntensity = reader.read(light, "flicker_intensity", 0.20f, 0.0f, 1.0f);
  outDef.lightBurstDurationMs = reader.read<uint32_t>(light, "burst_duration_ms", 150, kMinDurationMs, kMaxDurationMs);

  corrections = reader.corrections();
  return ESP_OK;
}

#ifndef NDEBUG
esp_err_t ProfileParser::runSelfTest() {
  static const char *testTag = "ParserTest";
  ESP_LOGI(testTag, "Running parser self-tests...");

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

  size_t corrections = 0;
  Inertial::InertialDefinition def{};
  if (parse(testJson, def, Diagnostics::Silent, corrections) != ESP_OK) {
    ESP_LOGE(testTag, "Test parse failed");
    return ESP_FAIL;
  }
  if (corrections != 0) return ESP_FAIL;

  if (def.profileName != "test_sith" || def.profileRoot != "profiles/sith/") return ESP_FAIL;
  if (def.kineticEnergyDeadbandG != 0.35f || def.rotationDeadbandDps != 18.0f) return ESP_FAIL;
  if (def.overloadThresholdG != 1.5f || def.overloadChargeRate != 2.5f) return ESP_FAIL;
  if (def.swingIdleThresholdG != 0.2f || def.humBaseVolume != 9000 || def.clashThresholdG != 2.5f) return ESP_FAIL;
  if (def.fontCounts.hum != 2 || def.fontCounts.swingPair != 4 || def.fontCounts.dragEnd != 3) return ESP_FAIL;
  if (def.ignitionDurationMs != 700 || def.retractionDurationMs != 400) return ESP_FAIL;
  if (def.blasterLedCount != 4 || def.dragLedCount != 6) return ESP_FAIL;
  if (def.bladeBaseHue != 120 || def.lightIdleBaseFreq != 1.5f) return ESP_FAIL;

  Inertial::InertialDefinition fallbackDef{};
  if (parse("{}", fallbackDef, Diagnostics::Silent, corrections) != ESP_OK) {
    ESP_LOGE(testTag, "Fallback test parse failed");
    return ESP_FAIL;
  }
  if (corrections != 0) return ESP_FAIL;

  if (fallbackDef.profileName != "unnamed" || fallbackDef.profileRoot != "profiles/unnamed/") return ESP_FAIL;
  if (fallbackDef.overloadThresholdG != 1.0f) return ESP_FAIL;
  if (fallbackDef.kineticEnergyDeadbandG != 0.25f || fallbackDef.rotationDeadbandDps != 15.0f) return ESP_FAIL;
  if (fallbackDef.fontCounts.hum != 1 || fallbackDef.bladeBaseHue != 240 || fallbackDef.clashThresholdG != 2.0f) return ESP_FAIL;

  const char *invalidJson = R"({
    "swing": {
      "idle_threshold_g": 0.5,
      "max_threshold_g": 0.5,
      "crossfade_low_g": 0.8,
      "crossfade_high_g": 0.3,
      "hum_base_volume": 70000,
      "clash_threshold_g": -1
    },
    "font_counts": { "swing_pair": 50, "blaster": 300 },
    "blade_timings": { "ignition_duration_ms": -5 },
    "blade_leds": { "blaster_count": 0 },
    "light": {
      "blade_base_hue": 400,
      "idle_base_freq": 0.2,
      "idle_pulse_depth": 1.5,
      "flicker_intensity": "high"
    }
  })";

  Inertial::InertialDefinition invalidDef{};
  if (parse(invalidJson, invalidDef, Diagnostics::Silent, corrections) != ESP_OK) {
    ESP_LOGE(testTag, "Invalid-value test parse failed");
    return ESP_FAIL;
  }
  if (corrections != 12) return ESP_FAIL;
  if (invalidDef.swingMaxThresholdG != 0.5f + kMinGSpan || invalidDef.swingCrossfadeHighG != 0.8f + kMinGSpan) return ESP_FAIL;
  if (invalidDef.humBaseVolume != Effects::kFullVolume || invalidDef.clashThresholdG != kMinClashThresholdG) return ESP_FAIL;
  if (invalidDef.fontCounts.swingPair != System::PsramAudioCache::kMaxSwingPairs || invalidDef.fontCounts.blaster != 255) return ESP_FAIL;
  if (invalidDef.ignitionDurationMs != kMinDurationMs || invalidDef.blasterLedCount != kMinLedCount) return ESP_FAIL;
  if (invalidDef.bladeBaseHue != kMaxHue || invalidDef.lightIdleBaseFreq != kMinIdleBaseFreqHz) return ESP_FAIL;
  if (invalidDef.lightIdlePulseDepth != 1.0f || invalidDef.lightFlickerIntensity != 0.20f) return ESP_FAIL;

  Inertial::InertialDefinition unslashedDef{};
  if (parse(R"({"root_path": "/profiles/sith"})", unslashedDef) != ESP_OK) {
    ESP_LOGE(testTag, "Unslashed root test parse failed");
    return ESP_FAIL;
  }
  const SoundFont unslashedFont(unslashedDef.profileRoot, unslashedDef.fontCounts);
  if (unslashedFont.root() != "profiles/sith/") return ESP_FAIL;
  if (unslashedFont.pathFor(FontCategory::Blaster, 1) != "/sdcard/profiles/sith/blst/blst1.wav") return ESP_FAIL;
  if (unslashedFont.selectionPath() != "/sdcard/profiles/sith/font.wav") return ESP_FAIL;

  if (SoundFont::normalizeRoot("a") != "a/" || SoundFont::normalizeRoot("a/") != "a/") return ESP_FAIL;
  if (SoundFont::normalizeRoot("/a/") != "a/" || SoundFont::normalizeRoot("a//") != "a/") return ESP_FAIL;
  if (!SoundFont::normalizeRoot("").empty()) return ESP_FAIL;

  ESP_LOGI(testTag, "All parser self-tests passed successfully!");
  return ESP_OK;
}
#endif

} // namespace InertialSaber::Profiles
