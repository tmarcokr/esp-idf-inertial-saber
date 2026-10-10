# Kinetic Effects Catalog: Discrete Events

InertialSaber OS defines "Effects" as discrete events triggered by specific sensor patterns or button gestures. These effects interact with the **InertialLight** and **InertialSwing** engines through LED overlays and one-shot sounds.

| Effect | Class | Priority | Trigger | Status |
| :--- | :--- | :---: | :--- | :--- |
| Kinetic Impact (Clash) | `KineticImpactEffect` | 2 | Kinetic energy drop | Implemented |
| Deflection Burst (Blaster) | `BlasterEffect` | 2 | 1 click while ignited | Implemented |
| Friction Burn (Drag) | `DragEffect` | 1 | Button hold while ignited | Implemented |
| Plasma Stabilization (Lockup) | — | — | — | Roadmap (not implemented) |
| Thrust Piercing (Stab) | — | — | — | Roadmap (not implemented) |

The priority only sets the evaluation order on the bus ([SaberAction](SaberAction.md) §4). All three implemented effects run only while the blade is **Ignited**. Sounds are picked at random among the files of their folder ([Profiles](Profiles.md) §2) and played at full volume. The durations and LED counts are per-profile keys ([Profiles](Profiles.md) §4).

## 1. Effect Philosophy: Reactive HSB
Unlike systems with fixed colors, effects in InertialSaber OS are **Relative** where it matters. They derive their colour from the profile's `BaseHue` (`light.blade_base_hue`) to ensure visual consistency.

- **White Flash:** Saturation set to 0.0.
- **Complementary Flash:** Hue shifted by 180°.
- **Thermal Shift:** Hue forced to the 25° - 45° range (Incandescence).

---

## 2. Catalog of Effects

### 2.1. Kinetic Impact (Clash)
- **Trigger:** Sudden drop of `kineticEnergy` larger than `swing.clash_threshold_g` (default **2.0 G**) within a **15 ms** window, while ignited.
  - **Window (implementation):** time-based. The effect keeps the kinetic energy of the IMU samples from the last **15 ms of IMU sample timestamps** (`SaberDataPacket::motionTimestampUs`), not a fixed number of bus cycles. A bus cycle that repeats an already-seen sample (timeout or input wake) adds nothing to the window. The drop is the peak inside the window minus the current sample. At the 100 Hz IMU rate the window spans 1–2 samples; this was validated on the reference board.
  - **Debounce:** at most one clash every **500 ms**.
- **Visual:** **Complementary Burst** (`BladeClashFlash`). The whole blade is blended with `Hue + 180°` at full saturation and brightness, fading back to the base color over `blade_timings.clash_duration_ms` (default 150 ms).
- **Audio:** A random `clsh/` one-shot.
- **Haptic:** Not implemented (see [InertialHaptics](InertialHaptics.md), roadmap).
- **Metrics:** `count,clash_detections` and `count,clash_retrigger_lt_1s` ([Diagnostics](Diagnostics.md)).

### 2.2. Deflection Burst (Blaster)
- **Trigger:** A **single click** while ignited (`Click` with `pressCount == 1`). The click resolves 400 ms after the press, or at the release for a press held between 400 and 500 ms; a press held 500 ms or longer is a drag, never a blaster. Two presses less than 400 ms apart are a double click, which retracts the blade, so consecutive blasters must be pressed more than 400 ms apart ([SaberAction](SaberAction.md) §2.2).
  - A motion-triggered blaster (energy pulse) is not implemented.
- **Visual:** **Localized Saturation Drop** (`BladeBlasterBlock`). A block of `blade_leds.blaster_count` LEDs (default 3) at a random position turns white for `blade_timings.blaster_duration_ms` (default 250 ms).
- **Audio:** A random `blst/` one-shot.

### 2.3. Friction Burn (Drag)
- **Trigger:** The button **held for 500 ms** while ignited (the first `HoldTick`). The drag lasts until the button is released.
  - A motion-triggered drag (blade pointing down with vibration) is not implemented.
- **Visual:** **Tip Incandescence** (`BladeDragEffect`). The last `blade_leds.drag_count` LEDs (default 8, limited to the strip) glow from HSB(25°) to HSB(45°) towards the tip with a random brightness flicker, blended over the blade. On release the glow fades out over 150 ms.
- **Audio:** A random looping `drag/` sound while held; on release it stops and a random `enddrag/` one-shot plays.
- **Haptic:** Not implemented (roadmap).

### 2.4. Plasma Stabilization (Lockup) — Roadmap
> **Not implemented.** Kept as a design target.

- **Trigger:** Detected high-frequency vibration (IMU noise) while `kineticEnergy` is low (Saber held against another).
- **Visual:** **Unstable Pulse.** The entire blade oscillates rapidly between `BaseHue` and `BaseHue + 20°`. Brightness jitters between 0.6 and 1.0.
- **Audio:** Aggressive, distorted plasma crackling.

### 2.5. Thrust Piercing (Stab) — Roadmap
> **Not implemented.** Kept as a design target.

- **Trigger:** High linear acceleration on the Y-axis (Forward) followed by a sudden stop.
- **Visual:** **Core Brightening.** The center of the blade turns white (`Saturation: 0.0`) while the outer edges intensify their base color.

---

## 3. Implementation Logic
Effects are `InertialEffect` objects added to the profile's effect set, which the **SaberAction Bus** evaluates every cycle.
1. **Evaluation:** The `test()` method checks if the sensor packet matches the trigger criteria (and the power state).
2. **Execution:** The `run()` method pushes an LED overlay to the SmartLed engine and queues the sound to the audio controller. It never blocks the bus.

New effects follow the same pattern; see `/create-effect` in the contributor tooling.
