---
description: Create and register a new power-on/power-off sequence (ignition/retraction) effect for a profile.
---

# Workflow: Create a New Power Effect

**Part of the Profile Creation Workflow Set.**
Use this workflow any time you need to implement a new power-on / power-off sequence for
a new InertialSaber OS profile. It documents the conventions established in the `inertial`
reference profile.

---

## Prerequisites

Before writing any code, activate the required skills:

1. **`ESP32_Expert`** — mandatory for all C++ code and FreeRTOS patterns.
2. **`saber-product-owner`** — mandatory if extending `InertialDefinition` with new fields.

---

## Step 1 — Prepare the Sound Font

The sound font directory for the new profile must follow the ProffieOS flat layout:

```
/sdcard/profiles/<profile-name>/
├── in/
│   ├── in1.wav
│   └── in2.wav   (add more as desired)
└── out/
    ├── out1.wav
    └── out2.wav  (add more as desired)
```

Measure the duration of every `in/` and `out/` file:

```bash
python3 -c "
import wave, os
for folder in ['in', 'out']:
    path = '/media/<device>/profiles/<profile-name>/' + folder
    for f in sorted(os.listdir(path)):
        if f.endswith('.wav'):
            with wave.open(os.path.join(path, f), 'r') as w:
                print(f'{folder}/{f}: {int(w.getnframes()/w.getframerate()*1000)} ms')
"
```

Record the counts and note any large duration spread in the `out/` files.

---

## Step 2 — Extend `InertialDefinition`

**File:** `main/core/models/InertialDefinition.hpp`

If not already present, ensure the definition contains:

```cpp
uint8_t fontInCount;           ///< Number of power-on sound files (in/in1.wav … inN.wav).
uint8_t fontOutCount;          ///< Number of power-off sound files (out/out1.wav … outN.wav).
uint32_t ignitionDurationMs;   ///< Hardcoded duration for the ignition sequence/animation.
uint32_t retractionDurationMs; ///< Hardcoded duration for the retraction sequence/animation.
```

> [!IMPORTANT]
> `InertialDefinition` is a POD struct. New fields must be added at the end of their
> logical group. Never reorder existing fields — all existing profiles initialize
> by field name (designated initializers), so missing new fields will be zero-initialized,
> not silently broken.

---

## Step 3 — Create the Blade Sweep Effects (reuse or extend)

**Files:**
* `main/profiles/<profile-name>/effects/BladeIgniteSweep.hpp` / `BladeIgniteSweep.cpp`
* `main/profiles/<profile-name>/effects/BladeRetractSweep.hpp` / `BladeRetractSweep.cpp`

For most profiles, copy `main/profiles/inertial/effects/BladeIgniteSweep.hpp/cpp` and `BladeRetractSweep.hpp/cpp` as-is.
The sweep color is derived from `InertialDefinition::bladeBaseHue` at construction time — no modification required for a different blade color.

If the new profile needs a non-standard sweep pattern (e.g., center-out ignition), implement a new `IEffect` subclass following the same contract:
- `update(delta_ms)`: advance elapsed time.
- `render(canvas)`: draw the current frame. Use `canvas.fillRange()` and `canvas.setPixel()`.
- `isFinished()`: return `true` when sweep is complete (triggers automatic overlay removal).

---

## Step 4 — Implement the Power Effect

**Files:**
* `main/profiles/<profile-name>/effects/PowerToggleEffect.hpp`
* `main/profiles/<profile-name>/effects/PowerToggleEffect.cpp`

For most profiles, copy `main/profiles/inertial/effects/PowerToggleEffect.hpp/cpp` and adjust only the `kSwingPreStartMs` constant if the in-sound duration warrants it.

### Key Constants/Fields to Review

| Field / Constant | Source / Default | Purpose |
|:---|:---|:---|
| `kSwingPreStartMs` | `100` (constant) | How early (ms before in-sound end) to start the engines. Increase for longer crossfade feel. |
| `ignitionDurationMs` | `InertialDefinition` | Duration of the ignition sound and animation in milliseconds (queries configuration directly, no filesystem read). |
| `retractionDurationMs` | `InertialDefinition` | Duration of the retraction sound and animation in milliseconds (queries configuration directly, no filesystem read). |

### Constructor Signature (do not change)

```cpp
PowerToggleEffect(
    InertialSwingEffect&                     swing,
    InertialLightEffect&                     light,
    Espressif::Wrappers::Audio::AudioEngine& audio,
    Espressif::Wrappers::SmartLed::Engine&   ledEngine,
    const Core::InertialDefinition&          definition,
    uint8_t                                  buttonId)
```

---

## Step 5 — Register in the Profile

**File:** `main/profiles/<profile-name>/<ProfileName>.cpp`

Inside the `load()` method, after registering `InertialSwingEffect` and
`InertialLightEffect`, add:

```cpp
bus.registerEffect(
    std::make_unique<Effects::PowerToggleEffect>(
        *swingEffect, *lightEffect, audio, led, kDefinition, /*buttonId=*/0));
```

Populate the counts and duration fields in the `InertialDefinition` literal:

```cpp
.fontInCount           = <N>,   // number of in/inN.wav files
.fontOutCount          = <M>,   // number of out/outN.wav files
.ignitionDurationMs   = <I>,   // duration of ignition in ms (e.g. 800)
.retractionDurationMs = <R>,   // duration of retraction in ms (e.g. 500)
```

---

## Step 6 — Verify the Build

```bash
idf.py build
```

Zero new warnings expected.

---

## Step 7 — Manual Validation on Hardware

1. Flash firmware.
2. Press button → confirm the correct `in/` sound plays, blade sweeps bottom-to-top.
3. Confirm engines (hum + breathing light) start slightly before the in-sound ends.
4. Press button again → confirm swing/hum stop immediately, `out/` sound plays,
   blade retracts top-to-bottom, blade goes dark at sound end.
5. Repeat 3–5 times to validate random file selection across the full `in/` and `out/` pools.

---

## State Machine Reference

```
IDLE_OFF ──[click]──► IGNITING
                         │ play in/inN.wav
                         │ pushOverlay(BladeIgniteSweep)
                         │ at (inDuration - kSwingPreStartMs): activate swing + light
                         ▼
                      IDLE_ON ──[click]──► RETRACTING
                                              │ deactivate swing + light immediately
                                              │ play out/outN.wav
                                              │ pushOverlay(BladeRetractSweep)
                                              ▼
                                           IDLE_OFF
```
