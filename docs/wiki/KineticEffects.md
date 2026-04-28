# Kinetic Effects Catalog: Discrete Events

InertialSaber OS defines "Effects" as discrete physical events triggered by specific sensor patterns. These effects interact with the **InertialLight** and **InertialSwing** engines using prioritized overrides.

## 1. Effect Philosophy: Reactive HSB
Unlike systems with fixed colors, effects in InertialSaber OS are **Relative**. They modify the current `BaseHue` of the profile to ensure visual consistency.

- **White Flash:** Saturation set to 0.0.
- **Complementary Flash:** Hue shifted by 180°.
- **Thermal Shift:** Hue forced to the 10° - 40° range (Incandescence).

---

## 2. Catalog of Effects

### 2.1. Kinetic Impact (Clash)
- **Trigger:** Sudden negative spike in `EnergiaTotal` (> 8.0G) within a 15ms window.
- **Visual:** **Complementary Burst.** Instant shift to `Hue + 180°`, `Brightness: 1.0`, `Saturation: 1.0`. Fades back to base color in 150ms.
- **Audio:** High-priority "Override" sound with sharp attack.
- **Haptic:** Single high-amplitude "Kinetic Kick" (Recoil).

### 2.2. Deflection Burst (Blaster)
- **Trigger:** Short, medium-intensity energy pulse (3.0G - 6.0G) or button press.
- **Visual:** **Localized Saturation Drop.** A localized section of the blade turns white (`Saturation: 0.0`) for 50ms.
- **Audio:** Sharp, high-frequency "ping" added to the mix.

### 2.3. Friction Burn (Drag)
- **Trigger:** `BladeAngle < -75°` AND `EnergiaTotal > 1.2G` (Sustained vibration).
- **Visual:** **Tip Incandescence.** The tip of the blade ignores `BaseHue` and shifts to **HSB(30, 1.0, 1.0)** (Orange Heat) with a chaotic flicker.
- **Audio:** Persistent low-frequency grinding sound.
- **Haptic:** Rough, low-frequency friction vibration.

### 2.4. Plasma Stabilization (Lockup)
- **Trigger:** Detected high-frequency vibration (IMU noise) while `EnergiaTotal` is low (Saber held against another).
- **Visual:** **Unstable Pulse.** The entire blade oscillates rapidly between `BaseHue` and `BaseHue + 20°`. Brightness jitters between 0.6 and 1.0.
- **Audio:** Aggressive, distorted plasma crackling.

### 2.5. Thrust Piercing (Stab)
- **Trigger:** High linear acceleration on the Y-axis (Forward) followed by a sudden stop.
- **Visual:** **Core Brightening.** The center of the blade turns white (`Saturation: 0.0`) while the outer edges intensify their base color.

---

## 3. Implementation Logic
Effects are registered as `InertialEffect` objects in the **SaberAction Bus**.
1. **Evaluation:** The `Test()` method checks if the sensor packet matches the trigger criteria.
2. **Execution:** The `Run()` method sends HSB modifiers to the Light Engine and triggers the Audio Player.
