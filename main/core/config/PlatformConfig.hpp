#pragma once

#include "sdkconfig.h"
#include <cstdint>

namespace InertialSaber::Core::Platform {

// ── Threading & Task Scheduling ─────────────────────────────────────────────
// These are hardware-level constants tied to the MCU topology. They are NOT
// per-profile. Per-profile parameters live in InertialDefinition.

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

// ── Sensor Processing ────────────────────────────────────────────────────────
/// Initial time after boot to ignore IMU data (allows filters to settle)
constexpr uint32_t kSensorGracePeriodMs = 3000;
/// Minimum linear acceleration in Gs to register as movement (filters out hand jitter)
constexpr float kKineticEnergyDeadbandG = 0.25f;
/// Minimum angular velocity in deg/sec to register as rotation
constexpr float kRotationDeadbandDps = 15.0f;
/// Calibration offset for the blade orientation angle in degrees
constexpr float kOrientationOffsetDeg = 0.0f;

// ── Inertial Overload Bus Defaults ───────────────────────────────────────────
// The SaberActionBus computes the Inertial Overload accumulator independently of
// the active profile, because it starts before any profile is loaded.
// Phase 3 will inject InertialDefinition into the bus so these defaults are
// overridden per profile at load time. Until then, these match the values in
// InertialDefaultProfile to preserve identical runtime behaviour.
constexpr float    kInertialOverloadThresholdG  = 1.0f;
constexpr float    kInertialOverloadChargeRate   = 2.0f;
constexpr float    kInertialOverloadDrainRate    = 0.5f;
constexpr float    kInertialBurstCooldownMs      = 1500.0f;

} // namespace InertialSaber::Core::Platform
