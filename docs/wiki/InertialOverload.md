# Feature: Inertial Overload & Inertial Burst

## 1. Physical Concept
The **Inertial Overload** represents the mechanical stress placed on the saber's virtual plasma containment field. Unlike `EnergiaTotal`, which is an instantaneous measurement of movement, the Inertial Overload is an **accumulator**. It requires sustained, high-energy movement to fill up. It acts as the physical embodiment of the user "pushing the saber to its limits" during prolonged combat or aggressive sweeping motions.

## 2. Functional Behavior
- **Charging**: When the user swings the saber intensely, the tank begins to fill.
- **Draining**: When the saber is held still or moved gently, the tank slowly dissipates its accumulated stress back to equilibrium.
- **The Climax (Inertial Burst)**: If the user sustains intense movement long enough to completely fill the tank (100%), the containment field momentarily breaches. This triggers an **Inertial Burst**, an instantaneous climax event that resets the tank and locks it in a cooldown state.

In terms of user experience:
- The rising **Inertial Overload** level can be mapped to an increasingly aggressive Audio hum or a visually destabilized LED blade (Plasma Rupture).
- The **Inertial Burst** is used to trigger a massive one-shot explosion sound and a bright, blinding flash on the blade.

## 3. Technical Specification

- **Inputs**: 
  - `KineticEnergy` (Instantaneous G-Force).
  - `dtSec` (Time elapsed since the last bus evaluation in seconds).
- **Core Formula (Delta Time Integration)**:
  - If `KineticEnergy > kOverloadChargeThresholdG`: 
    `InertialOverload += kOverloadChargeRatePerSec * dtSec`
  - Else:
    `InertialOverload -= kOverloadDrainRatePerSec * dtSec`
- **Output Bounds**: The accumulator is strictly clamped between `0.0f` (Rest) and `1.0f` (Full).

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

### 3.2. Technical Constants (`PlatformConfig.hpp`)
These parameters control the "feel" of the accumulation. 

| Parameter | Breadboard Value | Suggestion (Production) | Description |
| :--- | :--- | :--- | :--- |
| `kOverloadChargeThresholdG` | 1.0G | 3.5G | Energy floor to start charging. |
| `kOverloadChargeRatePerSec` | 2.0f | 0.5f | How fast the stress builds up. |
| `kOverloadDrainRatePerSec`  | 0.5f | 1.0f | How fast the stress dissipates. |
| `kOverloadBurstCooldownMs`  | 1500ms| 500ms - 1500ms | Recovery time after a burst. |

## 4. Synergy
The Inertial Overload is fully integrated into the `SaberDataPacket` distributed by the `SaberActionBus`. 
It operates synergistically alongside the `InertialSwing` engine. While `InertialSwing` maps immediate G-forces to volume, the **Inertial Overload** acts as a macro-modulator. It allows Flow Modulators to slowly transition their audio pitch or light patterns into a "stressed" state, while providing Event Triggers a reliable, physics-driven signature to fire discrete visual and auditory explosions.
