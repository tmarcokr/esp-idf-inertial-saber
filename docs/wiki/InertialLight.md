# InertialLight Engine: Visual Plasma Simulation

The **InertialLight Engine** translates kinetic metrics into organic light responses using the **HSB (Hue, Saturation, Brightness)** color space.

## 1. Visual Philosophy
The blade is treated as a dynamic column of plasma. Instead of static layers, its appearance is a continuous reaction to the physics metrics.

## 2. Visual States

### State A: Live Breathing (Idle/Calm)
**Condition:** `EnergiaTotal < 0.5 G` and `TanqueOverload == 0.0`.
The blade pulses gently, simulating a contained but living energy source.
- **Modulation:** The pulse frequency changes with `BladeAngle` (gravity).
- **Behavior:** Slower breathing when pointing down, faster when pointing up.

### State B: Thermal Excitacion (Movement)
**Condition:** `EnergiaTotal >= 0.5 G` or `TanqueOverload > 0.0`.
Physical movement "heats up" the plasma.
- **Thermal Bleed (Saturation):** As `TanqueOverload` increases, the saturation drops (the color turns white/hot).
- **Kinetic Flicker (Brightness):** High `EnergiaTotal` introduces chaotic, high-speed flickering in brightness.

### State C: Plasma Rupture (Burst)
**Condition:** Triggered when `TanqueOverload == 1.0`.
A violent release of energy that overrides all visual parameters for a brief moment.
- **Complementary Flash:** The Hue shifts +180° (complementary color) at 100% brightness and saturation for maximum contrast.
- **Fade Back:** After the burst, the system smoothly transitions back to the base profile color.

## 3. Core Formulas

### 1. Thermal Saturation:
`SaturationOut = 1.0 - (TanqueOverload * MaxThermalBleed)`
*(Where MaxThermalBleed is typically 0.8)*

### 2. Kinetic Flicker:
`BrightnessOut = 1.0 - (Random(-0.2, 0.2) * (EnergiaTotal / MaxG))`

### 3. Breathing Frequency:
`FreqOut = BaseFreq + (sin(BladeAngle) * 0.5)`
