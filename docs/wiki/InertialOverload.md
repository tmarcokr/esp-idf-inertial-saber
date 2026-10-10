# Feature: Inertial Overload & Inertial Burst

## 1. Physical Concept
The **Inertial Overload** represents the mechanical stress placed on the saber's virtual plasma containment field. Unlike `kineticEnergy`, which is an instantaneous measurement of movement, the Inertial Overload is an **accumulator**. It requires sustained, high-energy movement to fill up. It acts as the physical embodiment of the user "pushing the saber to its limits" during prolonged combat or aggressive sweeping motions.

## 2. Functional Behavior
- **Charging**: When the user swings the saber intensely, the tank begins to fill.
- **Draining**: When the saber is held still or moved gently, the tank slowly dissipates its accumulated stress back to equilibrium.
- **The Climax (Inertial Burst)**: If the user sustains intense movement long enough to completely fill the tank (100%), the containment field momentarily breaches. This triggers an **Inertial Burst**, an instantaneous climax event that resets the tank and locks it in a cooldown state.

In terms of user experience:
- The rising **Inertial Overload** level can be mapped to an increasingly aggressive Audio hum or a visually destabilized LED blade (Plasma Rupture).
- The **Inertial Burst** is used to trigger a massive one-shot explosion sound and a bright, blinding flash on the blade.

## 3. Technical Specification

- **Location**: computed by the bus on every cycle (`SaberActionBus::computeInertialOverload()`), before the effects are evaluated. It runs whether the blade is lit or not.
- **Inputs**: 
  - `kineticEnergy` (Instantaneous G-Force, after the `sensor.kinetic_deadband_g` deadband).
  - `dtSec` (Time elapsed since the last bus cycle in seconds).
- **Core Formula (Delta Time Integration)**:
  - If `kineticEnergy > threshold_g`: 
    `inertialOverload += charge_rate * dtSec`
  - Else:
    `inertialOverload -= drain_rate * dtSec`
- **Output Bounds**: The accumulator is strictly clamped between `0.0f` (Rest) and `1.0f` (Full).
- **Burst**: when the level reaches `1.0f`, `inertialBurst` is true for that one bus cycle, the level is reset to `0.0f`, and the cooldown starts. During `burst_cooldown_ms` the level stays at `0.0f` and does not charge.
- **Output**: `SaberDataPacket::inertialOverload` and `SaberDataPacket::inertialBurst` (see [SaberAction](SaberAction.md) §2.1).

### 3.1. Visual Flow Representation

```text
 ENERGY (G-Force)          TANK STATE (0 - 100%)               AUDIO ACTION
 ----------------          ----------------------------        ---------------
 G < 1.0G (Low)            [          empty           ]        Inertial Crossfade
                                 (Drain active)

 G = 1.2G (Medium)         [|||||||                   ]        Charging...
                             (Proportional filling)

 G = 1.7G (High)           [||||||||||||||||||||||||||] 100%   💥 INERTIAL BURST
                                 (Reset to 0)                  (Maximum Volume)
```

### 3.2. Profile Parameters (`profile.json` → `"overload"`)
These parameters control the "feel" of the accumulation. They are set per profile in the `"overload"` section of `profile.json` and applied by the bus when the profile becomes active (full schema in [Profiles](Profiles.md) §4.2).

| Key | Default | Range | Description |
| :--- | :--- | :--- | :--- |
| `threshold_g` | 1.0 G | 0 – 16 G | Energy floor to start charging. |
| `charge_rate` | 2.0 /s | 0 – 100 /s | How fast the stress builds up (1.0 = empty to full in 1 s). |
| `drain_rate` | 0.5 /s | 0 – 100 /s | How fast the stress dissipates. |
| `burst_cooldown_ms` | 1500 ms | 0 – 60000 ms | Recovery time after a burst. |

With the defaults, about 0.5 s of motion above 1.0 G fills an empty accumulator.

## 4. Synergy
The Inertial Overload is fully integrated into the `SaberDataPacket` distributed by the `SaberActionBus`. On a burst, `InertialSwingEffect` plays a random `swng/` one-shot and `InertialLightEffect` shows the Plasma Rupture flash.
It operates synergistically alongside the `InertialSwing` engine. While `InertialSwing` maps immediate G-forces to volume, the **Inertial Overload** acts as a macro-modulator. It allows Flow Modulators to slowly transition their audio pitch or light patterns into a "stressed" state, while providing Event Triggers a reliable, physics-driven signature to fire discrete visual and auditory explosions.
