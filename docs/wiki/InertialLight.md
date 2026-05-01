# InertialLight Engine: Visual Plasma Simulation

[Play with InertialSaber Simulator](https://tmarcokr.github.io/esp-idf-inertial-saber/)

## 1. System Overview
The **InertialLight Engine** is the visual counterpart to the *InertialSwing*. Its goal is to translate the kinetic forces and energy accumulation of the saber into organic light responses using the **HSB (Hue, Saturation, Brightness)** color space.

Visual behavior is no longer static or dependent on pre-recorded animations; it is a real-time response to the audio engine's metrics:
*   `EnergiaTotal` (Instantaneous G-Force).
*   `TanqueOverload` (Inertia accumulator from 0.0 to 1.0).
*   `BladeAngle` (Physical inclination of the saber).

---

## 2. Color Space Definition (HSB)
To facilitate temperature transitions and complementary bursts, all visual math is calculated in HSB.
*   **H (Hue):** 0-360 degrees. Defines the base color of the saber (e.g., 240 = Blue, 0 = Red, 120 = Green).
*   **S (Saturation):** 0.0 to 1.0. Determines purity. (1.0 = Pure color, 0.0 = Pure white).
*   **B (Brightness):** 0.0 to 1.0. Light intensity.

---

## 3. Visual States and Mathematics

### 3.1. State 1: "Live Breathing" (Idle/Calm)
**Condition:** `EnergiaTotal < 0.5 G` and `TanqueOverload == 0.0`
The saber is not off, but containing energy. It emits a soft pulse whose rhythm depends on gravity (Gravity Tonal Modulator).

**Formulas:**
1.  **Base breathing frequency:** `Freq = 1.0 Hz`
2.  **Gravity Modulator:** If the saber points up (+90°), it breathes faster (heat rises). If it points down (-90°), it breathes slower.
    `ActualFreq = Freq + (sin(BladeAngle) * 0.5)`
3.  **Brightness Calculation (Sine Wave):**
    `Pulse = (sin(Time * ActualFreq * 2 * PI) + 1.0) / 2.0`
    `FinalBrightness = 0.85 + (Pulse * 0.15)` *(Oscillates smoothly between 85% and 100%)*
4.  **Color:** `H = BaseHue`, `S = 1.0` (Pure color).

### 3.2. State 2: "Thermal Excitation" (Movement and Charging)
**Condition:** `EnergiaTotal >= 0.5 G` or `TanqueOverload > 0.0`
Movement pushes the brightness to maximum, while the inertia tank level "heats" the plasma, pushing the color towards white (loss of thermal saturation).

**Formulas:**
1.  **Reactive Brightness:** Brightness stays at 100%, but a chaotic flicker is injected based on instantaneous G-force.
    `Noise = Random(-1.0, 1.0) * clamp(EnergiaTotal / 4.0, 0.0, 1.0)`
    `FinalBrightness = clamp(1.0 - abs(Noise * 0.2), 0.8, 1.0)` *(Flickers between 80% and 100%)*
2.  **Plasma Heating (Saturation):** As the tank rises to 1.0 (100%), saturation drops by up to 80%, turning the saber core almost white.
    `FinalSaturation = 1.0 - (TanqueOverload * MaxThermalBleed)` *(where MaxThermalBleed is typically 0.8)*

### 3.3. State 3: "Plasma Rupture" (Inertial Burst)
**Condition:** Triggered instantly when `TanqueOverload == 1.0`
The violent release of energy produces a visual overload for a short frame/cycle (e.g., 100ms - 200ms) using the complementary color from color theory for maximum contrast.

**Formulas (Single Trigger):**
1.  **Complementary Color (180° Offset):**
    `BurstHue = (BaseHue + 180) % 360`
2.  **Saturation Restoration:** The flash must be a pure color to contrast with the previous white.
    `FinalSaturation = 1.0`
3.  **Super-Brightness:** If hardware allows (e.g., temporarily turning off channels to give more voltage to others), force maximum luminosity.
    `FinalBrightness = 1.0`

*Visual Note:* After the *BurstCooldown* time (e.g., 300ms defined in the audio engine), the light "fades" (smooth 150ms transition) back to the base color and the Idle state.

---

## 4. Light Tuning Summary (Parameters)

| Parameter | Suggested Value | Description |
| :--- | :--- | :--- |
| **IdlePulseDepth** | 0.15 (15%) | Depth of the oscillator in Idle state. |
| **IdleBaseFreq** | 1.0 Hz | Breathing cycles per second when horizontal. |
| **MaxThermalBleed** | 0.80 (80%) | How much saturation is lost at 100% Overload. |
| **BurstDuration** | 150ms | Visual duration of the Plasma Rupture flash. |
| **FlickerIntensity** | 0.20 (20%) | Chaos in brightness introduced by G-forces. |

## 5. Execution Logic (Pseudocode)

```text
// Loop executed at LED refresh rate (e.g., 100Hz)

Read metrics from InertialSwing (EnergiaTotal, TanqueOverload, BladeAngle)

IF (Plasma_Rupture_Event_Detected):
    Apply Complementary_Flash (Hue + 180, S=1.0, B=1.0)
    Start Burst_Timer

ELSE IF NOT (Burst_Timer_Active):
    IF (TanqueOverload > 0.0 OR EnergiaTotal > 0.5):
        // STATE 2: EXCITATION
        Hue = BaseHue
        Saturation = 1.0 - (TanqueOverload * MaxThermalBleed)
        Brightness = Calculate_G_Flicker(EnergiaTotal)
    ELSE:
        // STATE 1: BREATHING
        Hue = BaseHue
        Saturation = 1.0
        Brightness = Calculate_Gravity_Pulse(BladeAngle, Time)
        
Send to LED Controller (Convert HSB to RGB)
```