#pragma once

#include <cstdint>

namespace InertialSaber::Core {

/**
 * @brief Per-profile physics, audio, and visual configuration.
 *
 * Plain-data struct passed by const reference to engine effects at profile
 * load time. Decouples per-personality parameters from PlatformConfig.hpp,
 * which retains only hardware-level and task-scheduling constants.
 *
 * Field groups follow the three engine domains:
 *   - Inertial Overload accumulator mechanics
 *   - InertialSwing audio engine
 *   - InertialLight visual engine
 */
struct InertialDefinition {

    const char* profileName;   ///< Human-readable profile identifier.
    const char* profileRoot;   ///< Root path on SD relative to /sdcard/ (e.g. "profiles/inertial/").

    float overloadThresholdG;  ///< Minimum G-Force to start charging the accumulator.
    float overloadChargeRate;  ///< Accumulator fill rate per second above threshold.
    float overloadDrainRate;   ///< Accumulator drain rate per second at rest.
    float burstCooldownMs;     ///< Minimum milliseconds between successive Inertial Bursts.

    float    swingIdleThresholdG;   ///< Below this G-Force, swing volume is zero.
    float    swingMaxThresholdG;    ///< At or above this G-Force, swing volume is at maximum.
    float    swingCrossfadeLowG;    ///< Below this G-Force, SwingL dominates the tonal balance.
    float    swingCrossfadeHighG;   ///< Above this G-Force, SwingH dominates the tonal balance.
    float    gravityInfluence;      ///< Orientation influence on tonal balance (0.0–1.0).
    uint16_t humBaseVolume;         ///< Base hum volume (14-bit scale, 0–16384).
    float    humMaxDucking;         ///< Maximum hum reduction at full swing intensity (0.0–1.0).
    uint32_t swingSwapCooldownMs;   ///< Minimum idle time before a new swing pair can load.
    float    swingSwapMinVolume;    ///< Minimum master volume required to trigger a pair swap.

    uint8_t fontHumCount;          ///< Number of hum.wav files in the font directory.
    uint8_t fontSwingPairCount;    ///< Number of swingL/H pairs.
    uint8_t fontBurstCount;        ///< Number of burst one-shot files (swng1–N).
    uint8_t fontInCount;           ///< Number of power-on sound files (in/in1.wav … inN.wav).
    uint8_t fontOutCount;          ///< Number of power-off sound files (out/out1.wav … outN.wav).
    uint32_t ignitionDurationMs;   ///< Duration of the blade ignition sequence in milliseconds.
    uint32_t retractionDurationMs; ///< Duration of the blade retraction sequence in milliseconds.
    uint8_t fontBlasterCount;      ///< Number of blaster sound files.
    uint16_t blasterLedCount;      ///< Number of LEDs to light up for the blaster block.
    uint32_t blasterDurationMs;    ///< Duration of the blaster block visual effect in milliseconds.
    uint8_t fontClashCount;        ///< Number of clash sound files (clsh/clsh1.wav … clshN.wav).
    float clashThresholdG;         ///< Sudden negative spike/deceleration threshold in Gs.
    uint32_t clashDurationMs;      ///< Duration of the clash flash visual effect in milliseconds.

    uint16_t bladeBaseHue;         ///< Blade colour hue (HSB, 0–359). Blue = 240.
    float    lightIdleBaseFreq;    ///< Breathing cycles per second when horizontal.
    float    lightIdlePulseDepth;  ///< Oscillator depth in idle state (0.0–1.0).
    float    lightMaxThermalBleed; ///< Saturation loss at 100% Inertial Overload (0.0–1.0).
    float    lightFlickerIntensity;///< Brightness chaos introduced by G-forces (0.0–1.0).
    uint32_t lightBurstDurationMs; ///< Visual duration of the Plasma Rupture flash in ms.
};

} // namespace InertialSaber::Core
