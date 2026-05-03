# Technical Specification: InertialSwing Engine

[Play with InertialSaber Simulator](https://tmarcokr.github.io/esp-idf-inertial-saber/)

## 1. System Overview
The **InertialSwing Engine** is a high-performance audio model designed for the ESP32. This engine is based on **Linear Kinetic Energy** and **Physical Inertia**, allowing the saber to react to any movement (steps, thrusts, spins) in an organic and cinematic way.

The engine is divided into three subsystems:
1. **Inertial Crossfade**: Controls the volume and base mix according to the force of the movement.
2. **Gravity Tonal Modulator**: Adjusts the "color" of the sound according to the orientation of the saber.
3. **Zero-Volume SD Swapper**: Manages file loading on the SD card without generating noise or latency.

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



## 5. Mixer Integration (Final Formulas)

For implementation on the ESP32, the volume of each channel is calculated in each frame (800Hz) following this order of precedence:

### 5.1. Master Swing Volume Calculation
Determines the global presence of the movement sound based on total inertia.
`MasterVolume = clamp((EnergiaTotal - 0.5) / (4.0 - 0.5), 0.0, 1.0)`

### 5.2. Tonal Balance (Inertial Crossfade + Gravity)
Calculates the proportion between the two files loaded in memory.
1. `BaseMix = clamp((EnergiaTotal - 2.0) / (4.0 - 2.0), 0.0, 1.0)`
2. `GravityMod = sin(blade_angle) * 0.2`
3. `FinalMix = clamp(BaseMix + GravityMod, 0.0, 1.0)`

### 5.3. Output per Channel
`Volume_SwingL = MasterVolume * (1.0 - FinalMix)`
`Volume_SwingH = MasterVolume * FinalMix`

---

## 6. Tuning Notes

| Parameter | Default | Description |
| :--- | :--- | :--- |
| **GravityInfluence** | 0.2 (20%) | How much tilt affects the L/H balance. |


