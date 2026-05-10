#pragma once

#include "sdkconfig.h"
#include <cstdint>

namespace InertialSaber::Core::Platform {

#if CONFIG_IDF_TARGET_ESP32S3
    /// CPU core assigned to the SaberActionBus task
    constexpr int kBusTaskCore = 0;
    /// CPU core assigned to the Audio/LED Engine tasks
    constexpr int kEngineTaskCore = 1;
#else
    // ESP32-C6 and other single-core targets
    /// CPU core assigned to the SaberActionBus task
    constexpr int kBusTaskCore = 0;
    /// CPU core assigned to the Audio/LED Engine tasks
    constexpr int kEngineTaskCore = 0;
#endif

/// FreeRTOS priority for the SaberActionBus task (higher is more critical)
constexpr uint8_t kBusTaskPriority = 8;
/// Stack size allocated for the SaberActionBus task in bytes
constexpr uint32_t kBusTaskStackSize = 8192;
/// Maximum number of input peripherals (buttons, switches) supported
constexpr uint8_t kMaxInputs = 4;
/// Timeout in milliseconds for the bus loop when waiting for input notifications
constexpr uint32_t kBusTimeoutMs = 10;
/// Number of input events that can be safely queued before blocking
constexpr uint8_t kInputQueueDepth = 8;

// Sensor Processing Thresholds
/// Initial time after boot to ignore IMU data (allows filters to settle)
constexpr uint32_t kSensorGracePeriodMs = 3000;
/// Minimum linear acceleration in Gs to register as movement (filters out hand jitter)
constexpr float kKineticEnergyDeadbandG = 0.25f;
/// Minimum angular velocity in deg/sec to register as rotation
constexpr float kRotationDeadbandDps = 15.0f;
/// Calibration offset for the blade orientation angle in degrees
constexpr float kOrientationOffsetDeg = 0.0f;

// Inertial Overload Mechanics Default Thresholds
/// Minimum G-Force required to start filling the Inertial Overload accumulator
constexpr float kInertialOverloadThresholdG = 1.0f;
/// How much the Inertial Overload fills per second while swinging above the threshold
constexpr float kInertialOverloadChargeRate = 2.0f;
/// How much the Inertial Overload drains per second when the saber is resting
constexpr float kInertialOverloadDrainRate = 0.5f;
/// Minimum time in milliseconds between triggered Inertial Bursts
constexpr float kInertialBurstCooldownMs = 1500.0f;

// InertialSwing Engine Thresholds (See: docs/wiki/InertialSwing.md §5)
/// Below this G-Force, swing volume is zero (idle/calm state)
constexpr float kSwingIdleThresholdG = 0.15f;
/// At or above this G-Force, swing volume is at maximum
constexpr float kSwingMaxThresholdG = 1.0f;
/// Below this G-Force, SwingL dominates the tonal balance
constexpr float kSwingCrossfadeLowG = 0.4f;
/// Above this G-Force, SwingH dominates the tonal balance
constexpr float kSwingCrossfadeHighG = 1.0f;
/// Minimum master volume (0.0 - 1.0) required to trigger a pair swap when the swing stops
constexpr float kSwingSwapMinVolume = 0.40f;
/// Gravity orientation influence on tonal balance (0.0–1.0)
constexpr float kGravityInfluence = 0.2f;
/// Base hum volume (14-bit scale, 0–16384). Lowered to 8000 to leave headroom for loud swings.
constexpr uint16_t kHumBaseVolume = 8000;
/// Maximum hum reduction at full swing intensity (0.0–1.0)
constexpr float kHumMaxDucking = 0.75f;
/// Minimum time in milliseconds the saber must be idle before a new swing pair can be loaded
constexpr uint32_t kSwingSwapCooldownMs = 1000;

// InertialSwing Sound Font File Counts
/// Number of hum.wav files available in the font
constexpr uint8_t kFontHumCount = 1;
/// Number of swingL/H pairs (swingl1-N + swingh1-N)
constexpr uint8_t kFontSwingPairCount = 3;
/// Number of burst one-shot files (swng1-N)
constexpr uint8_t kFontBurstCount = 16;

// InertialLight Engine Thresholds (See: docs/wiki/InertialLight.md)
/// Breathing cycles per second when horizontal
constexpr float kLightIdleBaseFreq = 1.0f;
/// Depth of the oscillator in Idle state (0.0–1.0)
constexpr float kLightIdlePulseDepth = 0.15f;
/// How much saturation is lost at 100% Overload
constexpr float kLightMaxThermalBleed = 0.80f;
/// Chaos in brightness introduced by G-forces
constexpr float kLightFlickerIntensity = 0.20f;
/// Visual duration of the Plasma Rupture flash in milliseconds
constexpr uint32_t kLightBurstDurationMs = 150;

} // namespace InertialSaber::Core::Platform
