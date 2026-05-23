# Workflow: Create a New Clash Effect

**Part of the Profile Creation Workflow Set.**
Use this workflow any time you need to implement a clash effect for a new InertialSaber OS profile. It documents the conventions established in the `inertial` reference profile.

---

## Prerequisites

Before writing any code, activate the required skills:

1. **`ESP32_Expert`** — mandatory for all C++ code and FreeRTOS patterns.
2. **`saber-product-owner`** — mandatory if extending `InertialDefinition` with new fields.

---

## Step 1 — Prepare the Sound Font

The sound font directory for the new profile must contain clash sound files under the `clsh` folder relative to the profile root:

```
/sdcard/profiles/<profile-name>/
└── clsh/
    ├── clsh1.wav
    ├── clsh2.wav
    └── clsh3.wav  (add more as desired)
```

Count the total number of `clsh*.wav` files. Let this count be $N$.

---

## Step 2 — Extend `InertialDefinition`

**File:** `main/core/models/InertialDefinition.hpp`

If not already present, ensure the definition contains the following properties at the end of the sound counts/durations group:

```cpp
uint8_t fontClashCount;        ///< Number of clash sound files.
float clashThresholdG;         ///< Sudden negative spike/deceleration threshold in Gs.
uint32_t clashDurationMs;      ///< Duration of the clash flash visual effect in milliseconds.
```

---

## Step 3 — Create the Clash LED Overlay

**Files:**
* `main/profiles/<profile-name>/effects/BladeClashFlash.hpp`
* `main/profiles/<profile-name>/effects/BladeClashFlash.cpp`

For most profiles, reuse `main/profiles/inertial/effects/BladeClashFlash.hpp/cpp`.

The clash overlay class inherits from `Espressif::Wrappers::SmartLed::IEffect` and performs the following tasks:
- **Construction**: Computes the complementary hue: `m_clashHue = (hue + 180) % 360`.
- **Render**: Fades the complementary color over the entire blade canvas over `clashDurationMs`. Uses `canvas.blendPixel()` with alpha decreasing from 255 down to 0:
  ```cpp
  float progress = static_cast<float>(m_elapsed) / m_durationMs;
  uint8_t alpha = static_cast<uint8_t>(255.0f * (1.0f - progress));
  Color clashColor = hsvToRgb(m_clashHue, 255, 255);
  for (uint16_t i = 0; i < canvas.size(); ++i) {
      canvas.blendPixel(i, clashColor, alpha);
  }
  ```
- **Termination**: Automatically finishes and self-removes from the overlay pool after `clashDurationMs`.

---

## Step 4 — Implement the Clash Action Effect

**Files:**
* `main/profiles/<profile-name>/effects/KineticImpactEffect.hpp`
* `main/profiles/<profile-name>/effects/KineticImpactEffect.cpp`

For most profiles, reuse `main/profiles/inertial/effects/KineticImpactEffect.hpp/cpp`.

### Key Trigger Logic in `Test()`

The clash effect triggers under the following conditions:
1. The saber is fully active (queried via `m_power.isActive()`).
2. A sudden drop (negative spike) in `KineticEnergy` greater than the clash threshold occurs within a 15ms window (3-4 cycles in the 800Hz/200Hz loop).
3. A cooldown period of 500ms has elapsed since the last clash to prevent double-triggering.

Keep a history buffer of the last 4 samples of `KineticEnergy` to detect the drop:
```cpp
bool KineticImpactEffect::Test(const Core::SaberDataPacket &packet) {
  if (!m_power.isActive()) {
    m_history.fill(0.0f);
    return false;
  }

  // Push to history
  m_history[m_historyIdx] = packet.KineticEnergy;
  m_historyIdx = (m_historyIdx + 1) % m_history.size();

  // Find max in the window
  float maxVal = 0.0f;
  for (float val : m_history) {
    if (val > maxVal) maxVal = val;
  }

  // Check drop
  float drop = maxVal - packet.KineticEnergy;
  if (drop > m_def.clashThresholdG && (packet.timestamp_ms - m_lastClashTimeMs) > 500) {
    m_lastClashTimeMs = packet.timestamp_ms;
    return true;
  }

  return false;
}
```

### Key Execution Logic in `Run()`

When triggered, the effect:
1. Selects a random clash sound file index from $0$ to $N-1$.
2. Instructs the `AudioEngine` to play the corresponding `clsh/clsh*.wav` file.
3. Pushes a `BladeClashFlash` visual overlay onto the LED engine.

---

## Step 5 — Register in the Profile

**File:** `main/profiles/<profile-name>/<ProfileName>.cpp`

Inside the `load()` method, register the `KineticImpactEffect` passing the reference to the registered `PowerToggleEffect` instance:

```cpp
auto powerFx = std::make_unique<Effects::PowerToggleEffect>(
    *swingEffect, *lightEffect, audio, led, kDefinition, 0);
auto &powerRef = *powerFx;
bus.registerEffect(std::move(powerFx));

bus.registerEffect(std::make_unique<Effects::KineticImpactEffect>(
    powerRef, audio, led, kDefinition));
```

Populate the clash fields in the `InertialDefinition` literal:

```cpp
.fontClashCount   = <N>,   // e.g. 16
.clashThresholdG  = 8.0f,  // 8.0G threshold
.clashDurationMs  = 150,   // 150ms fade duration
```

---

## Step 6 — Verify the Build

```bash
idf.py build
```

Verify that there are no compilation errors or warnings.

---

## Step 7 — Manual Validation on Hardware

1. Flash the firmware.
2. Turn the saber ON (click the button once).
3. Tap the saber hilt firmly against your hand or a soft surface → verify a random `clsh/*.wav` sound plays and the entire blade flashes with a complementary color (e.g. orange if base is blue) and fades back to base within 150ms.
4. Verify swings do not trigger accidental clashes, only firm physical taps/impacts trigger the effect.
