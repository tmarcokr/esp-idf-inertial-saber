# InertialSaber OS: Core Functional Framework

## 1. The Metric System (Input)
All functionality is driven by three primary real-time metrics:
- **EnergiaTotal (G):** Vector magnitude of linear acceleration.
- **TanqueOverload (0.0 - 1.0):** Integration of energy over time; represents accumulated inertia.
- **BladeAngle (Degrees):** Physical orientation relative to gravity.

## 2. InertialSwing Engine (Audio)
- **Inertial Crossfade:** Mixing `swingL` (Low) and `swingH` (High) based on `EnergiaTotal`.
- **Inertial Overload:** Discharging high-energy sound bursts when the tank is full.
- **Gravity Tonal Modulator:** Shifting the tonal balance based on `BladeAngle`.
- **Zero-Volume Swapper:** Loading new sounds from SD only when the system is silent to avoid latency.

## 3. InertialLight Engine (Visual)
- **HSB Space:** All color math is calculated in Hue, Saturation, Brightness.
- **Respiración Viva (Calma):** Low-energy pulse modulated by `BladeAngle`.
- **Excitación Térmica (Movement):** Loss of saturation (whitening) and chaotic flicker based on `EnergiaTotal`.
- **Rotura de Plasma (Burst):** Complementary color flash triggered by `TanqueOverload == 1.0`.

## 4. Defining New Features (Template)
When defining a new feature, follow this structure:
1. **Physical Trigger:** Which metric triggers the effect?
2. **Functional Behavior:** What happens conceptually?
3. **Technical Formula:** How is the output calculated? (e.g., `Output = Metric * Gain + Offset`)
4. **Originality Check:** Does it reference legacy systems? (If yes, rewrite).
