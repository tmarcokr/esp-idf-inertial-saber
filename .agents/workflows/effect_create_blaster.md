---
description: Create and register a new blaster block effect (sound and visual overlay) for a profile.
---

# Workflow: Create a New Blaster Block Effect

**Part of the Profile Creation Workflow Set.**
Use this workflow any time you need to implement a blaster block effect for a new InertialSaber OS profile. It documents the conventions established in the `inertial` reference profile.

---

## Prerequisites

Before writing any code, activate the required skills:

1. **`ESP32_Expert`** — mandatory for all C++ code and FreeRTOS patterns.
2. **`saber-product-owner`** — mandatory if extending `InertialDefinition` with new fields.

---

## Step 1 — Prepare the Sound Font

The sound font directory for the new profile must contain blaster sound files under the `blst` folder relative to the profile root:

```
/sdcard/profiles/<profile-name>/
└── blst/
    ├── blst1.wav
    ├── blst2.wav
    └── blst3.wav  (add more as desired)
```

Count the total number of `blst*.wav` files. Let this count be $N$.

---

## Step 2 — Extend `InertialDefinition`

**File:** `main/core/models/InertialDefinition.hpp`

If not already present, ensure the definition contains the following properties at the end of the sound counts/durations group:

```cpp
uint8_t fontBlasterCount;      ///< Number of blaster sound files.
uint16_t blasterLedCount;      ///< Number of LEDs to light up for the blaster block.
uint32_t blasterDurationMs;    ///< Duration of the blaster block visual effect in milliseconds.
```

---

## Step 3 — Create or Reuse the Blaster LED Overlay

**Files:**
* `main/profiles/<profile-name>/effects/BladeBlasterBlock.hpp` / `BladeBlasterBlock.cpp`

For most profiles, reuse `main/profiles/inertial/effects/BladeBlasterBlock.hpp/cpp` as-is. 

The blaster block overlay class inherits from `Espressif::Wrappers::SmartLed::IEffect` and performs the following tasks:
- **Construction**: Selects a random starting pixel on the blade canvas within valid boundaries (`0` to `numLeds - blasterLedCount`).
- **Render**: Fills that contiguous block of `blasterLedCount` LEDs with `Color::White()`.
- **Termination**: Automatically finishes and self-removes from the overlay pool after `blasterDurationMs`.

---

## Step 4 — Implement the Blaster Action Effect

**Files:**
* `main/profiles/<profile-name>/effects/BlasterEffect.hpp`
* `main/profiles/<profile-name>/effects/BlasterEffect.cpp`

For most profiles, reuse `main/profiles/inertial/effects/BlasterEffect.hpp/cpp`.

### Key Trigger Logic in `Test()`

The blaster effect triggers under the following conditions:
1. The saber is fully active (queried via `m_power.isActive()`).
2. A single click is detected (release with duration < 500ms).

Double-clicks are filtered out automatically because `PowerToggleEffect` runs before `BlasterEffect` (via priority order: `PowerToggleEffect` has Priority = 1, `BlasterEffect` has Priority = 2). When the second click of a double-click is processed, `PowerToggleEffect` immediately transitions to the `RETRACTING` state, causing `m_power.isActive()` to return `false` when `BlasterEffect::Test` is subsequently evaluated.

```cpp
bool BlasterEffect::Test(const Core::SaberDataPacket &packet) {
  if (!m_power.isActive()) {
    return false;
  }
  if (m_buttonId >= Core::Platform::kMaxInputs) {
    return false;
  }

  const auto &input = packet.inputs[m_buttonId];
  using InputState = Core::InputDescriptor::State;

  return (input.current == InputState::RELEASED &&
          input.previous != InputState::IDLE &&
          input.holdDuration_ms < 500);
}
```

### Key Execution Logic in `Run()`

When triggered, the effect:
1. Selects a random blaster sound file index from $0$ to $N-1$.
2. Instructs the `AudioEngine` to play the corresponding `.wav` file.
3. Pushes a `BladeBlasterBlock` visual overlay onto the LED engine.

---

## Step 5 — Register in the Profile

**File:** `main/profiles/<profile-name>/<ProfileName>.cpp`

Inside the `load()` method, register the `BlasterEffect` by passing the reference to the registered `PowerToggleEffect` instance:

```cpp
auto powerFx = std::make_unique<Effects::PowerToggleEffect>(
    *swingEffect, *lightEffect, audio, led, kDefinition, 0);
auto &powerRef = *powerFx;
bus.registerEffect(std::move(powerFx));

bus.registerEffect(std::make_unique<Effects::BlasterEffect>(
    powerRef, audio, led, kDefinition, 0));
```

Populate the blaster fields in the `InertialDefinition` literal:

```cpp
.fontBlasterCount  = <N>,   // e.g. 8
.blasterLedCount   = <L>,   // e.g. 3
.blasterDurationMs = <D>,   // e.g. 250
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
3. With the saber ON, click the button once (short click) → verify a random `blst/*.wav` sound plays and a brief white spot of 3 LEDs flashes at a random position on the blade.
4. With the saber ON, click the button twice quickly (double click with < 500ms interval) → verify the saber retracts and turns OFF (first click starts the blaster block, and the second click immediately initiates retraction, cutting off the blaster block).
