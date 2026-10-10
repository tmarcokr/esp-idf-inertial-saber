# InertialLight Engine: Visual Plasma Simulation

## 1. System Overview
The **InertialLight Engine** is the visual counterpart to the *InertialSwing*. Its goal is to translate the kinetic forces and energy accumulation of the saber into organic light responses using the **HSB (Hue, Saturation, Brightness)** color space.

Visual behavior is no longer static or dependent on pre-recorded animations; it is a real-time response to the kinetic metrics of the bus packet (see [Kinetic Metrics](KineticMetrics.md)):
*   `kineticEnergy` (Instantaneous G-Force, in G).
*   `inertialOverload` (Inertia accumulator from 0.0 to 1.0) and `inertialBurst`.
*   `orientation` (Blade inclination, −1.0 straight down … +1.0 straight up).

It is implemented by `InertialLightEffect` (`main/profiles/inertial/effects/`), a flow modulator evaluated on every bus cycle while the blade is ignited. It writes the blade colour into a base LED effect (`InertialBladeEffect`) that the SmartLed engine renders at 100 frames per second; the Plasma Rupture is pushed as an overlay. Every parameter in §4 is a per-profile key of the `light` section of `profile.json` ([Profiles](Profiles.md) §4.8).

---

## 2. Color Space Definition (HSB)
To facilitate temperature transitions and complementary bursts, all visual math is calculated in HSB.
*   **H (Hue):** 0-360 degrees. Defines the base color of the saber (e.g., 240 = Blue, 0 = Red, 120 = Green).
*   **S (Saturation):** 0.0 to 1.0. Determines purity. (1.0 = Pure color, 0.0 = Pure white).
*   **B (Brightness):** 0.0 to 1.0. Light intensity.

---

## 3. Visual States and Mathematics

### 3.1. State 1: "Live Breathing" (Idle/Calm)
**Condition:** `kineticEnergy < 0.5 G` and `inertialOverload == 0.0`
The saber is not off, but containing energy. It emits a soft pulse whose rhythm depends on gravity (Gravity Tonal Modulator).

**Formulas:**
1.  **Base breathing frequency:** `Freq = idle_base_freq` (default 1.0 Hz)
2.  **Gravity Modulator:** If the saber points up (`orientation` +1.0), it breathes faster (heat rises). If it points down (−1.0), it breathes slower.
    `ActualFreq = Freq + (sin(orientation * PI / 2) * 0.5)`
3.  **Brightness Calculation (Sine Wave):**
    `Pulse = (sin(Time * ActualFreq * 2 * PI) + 1.0) / 2.0`
    `FinalBrightness = (1.0 - idle_pulse_depth) + (Pulse * idle_pulse_depth)` *(with the default depth 0.15, oscillates smoothly between 85% and 100%)*
4.  **Color:** `H = BaseHue`, `S = 1.0` (Pure color).

### 3.2. State 2: "Thermal Excitation" (Movement and Charging)
**Condition:** `kineticEnergy >= 0.5 G` or `inertialOverload > 0.0`
Movement pushes the brightness to maximum, while the inertia tank level "heats" the plasma, pushing the color towards white (loss of thermal saturation).

**Formulas:**
1.  **Reactive Brightness:** Brightness stays at 100%, but a chaotic flicker is injected based on instantaneous G-force.
    `Noise = Random(-1.0, 1.0) * clamp(kineticEnergy / 4.0, 0.0, 1.0)`
    `FinalBrightness = clamp(1.0 - abs(Noise * flicker_intensity), 0.8, 1.0)` *(Flickers between 80% and 100%)*
2.  **Plasma Heating (Saturation):** As the tank rises to 1.0 (100%), saturation drops by up to 80%, turning the saber core almost white.
    `FinalSaturation = 1.0 - (inertialOverload * max_thermal_bleed)` *(default `max_thermal_bleed` 0.8)*

### 3.3. State 3: "Plasma Rupture" (Inertial Burst)
**Condition:** Triggered instantly when `inertialBurst` is true (the Inertial Overload reached 1.0).
The violent release of energy produces a visual overload for a short time (`burst_duration_ms`, default 150 ms) using the complementary color from color theory for maximum contrast.

**Formulas (Single Trigger):**
1.  **Complementary Color (180° Offset):**
    `BurstHue = (BaseHue + 180) % 360`
2.  **Saturation Restoration:** The flash must be a pure color to contrast with the previous white.
    `FinalSaturation = 1.0`
3.  **Super-Brightness:** Maximum luminosity.
    `FinalBrightness = 1.0`

*Visual Note:* The flash is an overlay on top of the blade and ends after `burst_duration_ms`. During the burst cooldown (`overload.burst_cooldown_ms`, default 1500 ms, applied by the bus, see [Inertial Overload](InertialOverload.md)) the accumulator stays at 0, so no new burst can fire and the saturation returns to the base color.

---

## 4. Light Tuning Summary (Parameters)

| Parameter (`light.*`) | Default | Description |
| :--- | :--- | :--- |
| `blade_base_hue` | 240 (blue) | Base hue of the blade (0–359). |
| `idle_pulse_depth` | 0.15 (15%) | Depth of the oscillator in Idle state. |
| `idle_base_freq` | 1.0 Hz | Breathing cycles per second when horizontal. |
| `max_thermal_bleed` | 0.80 (80%) | How much saturation is lost at 100% Overload. |
| `burst_duration_ms` | 150 ms | Visual duration of the Plasma Rupture flash. |
| `flicker_intensity` | 0.20 (20%) | Chaos in brightness introduced by G-forces. |

Fixed in the code (not per profile): the excitation threshold (0.5 G), the flicker full scale (4.0 G), the minimum flicker brightness (80%) and the gravity modulation of the breathing rate (±0.5 Hz).

## 5. Execution Logic (Pseudocode)

```text
// Executed on every bus cycle while ignited; the LED engine renders at 100 fps

Read metrics from the bus packet (kineticEnergy, inertialOverload, inertialBurst, orientation)

IF (inertialOverload > 0.0 OR kineticEnergy >= 0.5):
    // STATE 2: EXCITATION
    Hue = BaseHue
    Saturation = 1.0 - (inertialOverload * MaxThermalBleed)
    Brightness = Calculate_G_Flicker(kineticEnergy)
ELSE:
    // STATE 1: BREATHING
    Hue = BaseHue
    Saturation = 1.0
    Brightness = Calculate_Gravity_Pulse(orientation, Time)

Send to the base blade effect (HSB, converted to RGB by the LED engine)

IF (inertialBurst):
    // STATE 3: PLASMA RUPTURE, drawn on top of the base effect
    Push Complementary_Flash overlay (Hue + 180, S=1.0, B=1.0, burst_duration_ms)
```