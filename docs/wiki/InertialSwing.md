# Technical Specification: InertialSwing Engine

[Play with InertialSaber Simulator](https://tmarcokr.github.io/esp-idf-inertial-saber/)

## 1. System Overview
The **InertialSwing Engine** is a high-performance audio model designed for the ESP32. This engine is based on **Linear Kinetic Energy** and **Physical Inertia**, allowing the saber to react to any movement (steps, thrusts, spins) in an organic and cinematic way.

The engine is divided into four subsystems:
1. **Inertial Crossfade**: Controls the volume and base mix according to the force of the movement.
2. **Inertial Overload**: An accumulator that triggers bursts of aggressive sound after real physical effort.
3. **Gravity Tonal Modulator**: Adjusts the "color" of the sound according to the orientation of the saber.
4. **Zero-Volume SD Swapper**: Manages file loading on the SD card without generating noise or latency.

---

## 2. Energy and Mix Calculation (Inertial Crossfade)
The system measures the total energy of the movement by summing the 3D acceleration vectors (subtracting gravity).

**Energy Formula:**
`EnergiaTotal = sqrt(mss_x^2 + mss_y^2 + mss_z^2)`

### 2.1. Intensity Thresholds
The `EnergiaTotal` defines the master volume and the balance between the `swingL` (Low/Bass) and `swingH` (High/Treble) files.

| State | Energy (G) | Mixer Action |
| :--- | :--- | :--- |
| **Idle (Calm)** | 0.0 - 0.5 G | Only Hum audible. Swing Volume = 0%. |
| **L Zone** | 0.5 - 2.0 G | `swingL` dominates. Master volume rising. |
| **Transition** | 2.0 - 4.0 G | Crossfade active: `swingL` goes down and `swingH` goes up. |
| **H Zone** | > 4.0 G | `swingH` dominates at maximum volume. |

---

## 3. Gravity Modulation (Gravity Tonal Modulator)
The angle of the blade (`blade_angle`) acts as an equalizer that "colors" the sound, giving more weight to one texture or another depending on the direction.

* **Saber Up (+90°):** Ethereal sound. Injects +20% weight to `swingH`.
* **Saber Horizontal (0°):** Neutral tone defined only by Intensity.
* **Saber Down (-90°):** Heavy/roaring sound. Injects +20% weight to `swingL`.

**Final Mix Calculation:**
1. `GravityModulator = sin(blade_angle)`
2. `BaseMix = clamp((EnergiaTotal - 2.0) / (4.0 - 2.0), 0.0, 1.0)`
3. `FinalMix = clamp(BaseMix + (GravityModulator * 0.2), 0.0, 1.0)`

---

## 4. File Management (Zero-Volume SD Swapper)
To avoid saturating the ESP32's I2C/SD bus, only one pair of files (`swingL/H`) is loaded at a time. The swap occurs in the "Invisible Moment".

### 4.1. Flowchart

```text
[ CALM ] ────────────( Energy >= 0.5G )────────────┐
  Volume: 0%                                       │
  SD: Ready                                        ▼
      ▲                                      [ MOVEMENT ]
      │                                       Volume: > 0%
      │                                       Mix active
      │                                            │
  ( INVISIBLE MOMENT )                             │
  1. Actual volume == 0                            ▼
  2. Close L1/H1                        [ FADE OUT ]
  3. Open L2/H2 from SD                  Volume drops to 0%
  4. State -> CALM                       Energy < 0.5G
      ▲                                            │
      └───────────( Volume == 0.0 )────────────────┘
```

## 5. Energy Overload (Inertial Overload)
Represents a virtual inertia tank that requires real physical effort to charge and is released explosively when the air "breaks" due to the speed of the blade.

### 5.1. Accumulator Logic
To avoid the machine-gun effect (repetitive shots), the system requires a sustained accumulation of force before generating a sonic burst.

1.  **Active Charge**: Only if the `EnergiaTotal` exceeds the **3.5 G threshold**, the tank begins to fill proportionally to the excess force applied.
2.  **Inertia Drain**: If the movement drops below 3.5 G, the tank empties quickly. This simulates the loss of inertia and prevents accumulated slow movements from triggering the effect by mistake.
3.  **Inertial Burst**: Upon reaching 100%, the engine sends an order to the polyphonic mixer to play a `swng` or `slsh` file and resets the tank to zero immediately.
4. **Cooldown**: After the Burst, the tank remains blocked for `BurstCooldown` ms before accepting a new charge.

### 5.2. Visual Flow Representation

```text
 ENERGY (G-Force)          TANK STATE (0 - 100%)               AUDIO ACTION
 ----------------          ----------------------------        ---------------
 G < 3.5G (Low)            [          empty           ]        Inertial Crossfade
                                 (Drain active)

 G = 4.5G (Medium)         [|||||||                   ]        Charging...
                             (Proportional filling)

 G = 7.0G (High)           [||||||||||||||||||||||||||] 100%   💥 INERTIAL BURST
                                 (Reset to 0)                  (Maximum Volume)
```

## 6. Mixer Integration (Final Formulas)

For implementation on the ESP32, the volume of each channel is calculated in each frame (800Hz) following this order of precedence:

### 6.1. Master Swing Volume Calculation
Determines the global presence of the movement sound based on total inertia.
`MasterVolume = clamp((EnergiaTotal - 0.5) / (4.0 - 0.5), 0.0, 1.0)`

### 6.2. Tonal Balance (Inertial Crossfade + Gravity)
Calculates the proportion between the two files loaded in memory.
1. `BaseMix = clamp((EnergiaTotal - 2.0) / (4.0 - 2.0), 0.0, 1.0)`
2. `GravityMod = sin(blade_angle) * 0.2`
3. `FinalMix = clamp(BaseMix + GravityMod, 0.0, 1.0)`

### 6.3. Output per Channel
`Volume_SwingL = MasterVolume * (1.0 - FinalMix)`
`Volume_SwingH = MasterVolume * FinalMix`

---

## 7. Tuning Notes

| Parameter | Suggested Value | Description |
| :--- | :--- | :--- |
| **OverloadSensitivity** | 5.0 | The higher the value, the less physical effort to trigger a Burst. |
| **OverloadDrainRate** | 10.0 | Drain speed. Must be > Filling to avoid erroneous accumulation. |
| **BurstCooldown** | 300ms | Minimum tank re-arming time after a successful trigger. |
| **GravityInfluence** | 0.2 (20%) | How much tilt affects the L/H balance. |
