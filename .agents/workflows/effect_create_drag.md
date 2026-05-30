# Workflow: Create a New Drag Effect

**Part of the Profile Creation Workflow Set.**
Use this workflow any time you need to implement a drag effect for a new InertialSaber OS profile. It documents the conventions established in the `inertial` reference profile.

---

## Prerequisites

Before writing any code, activate the required skills:

1. **`ESP32_Expert`** — mandatory for all C++ code and FreeRTOS patterns.
2. **`saber-product-owner`** — mandatory if extending `InertialDefinition` with new fields.

---

## Step 1 — Prepare the Sound Font

The sound font directory for the new profile must contain drag sound files under the `drag` and `enddrag` folders relative to the profile root:

```
/sdcard/profiles/<profile-name>/
├── drag/
│   ├── drag1.wav
│   └── drag2.wav   (looping drag sounds)
└── enddrag/
    ├── enddrag1.wav
    └── enddrag2.wav (deactivation sounds)
```

Count the total number of `drag*.wav` files ($N$) and `enddrag*.wav` files ($M$).

---

## Step 2 — Extend `InertialDefinition`

**File:** `main/core/models/InertialDefinition.hpp`

Ensure the definition contains the following properties:

```cpp
uint8_t fontDragCount;         ///< Number of looping drag sound files (drag/drag*.wav).
uint8_t fontDragEndCount;      ///< Number of deactivation drag sound files (enddrag/enddrag*.wav).
uint16_t dragLedCount;         ///< Number of LEDs to light up at the tip for the thermal glow.
```

---

## Step 3 — Extend GpioButton / InputAdapter

**File:** `main/system/adapters/InputAdapter.cpp`

Bind `onLongPress(500, ...)` to the main button to push the `State::HELD` event to the bus:

```cpp
m_mainButton.onLongPress(500, [this]() {
    m_btnState.previous = m_btnState.current;
    m_btnState.current = Core::InputDescriptor::State::HELD;
    m_btnState.holdDuration_ms = 500;
    m_bus.pushInputEvent(Config::HardwareConfig::kMainBtnInputId, m_btnState);
});
```

---

## Step 4 — Create the Drag LED Overlay

**Files:**
* `main/profiles/<profile-name>/effects/BladeDragEffect.hpp`
* `main/profiles/<profile-name>/effects/BladeDragEffect.cpp`

For most profiles, reuse `main/profiles/inertial/effects/BladeDragEffect.hpp/cpp`.

The drag overlay class inherits from `Espressif::Wrappers::SmartLed::IEffect` and performs the following tasks:
- **Sustained Render**: Blends an incandescent thermal gradient (red-orange to yellow-white with random flicker) over the top `dragLedCount` pixels of the blade.
- **Fade Out**: When `terminate()` is called, triggers a 150ms fade-out before ending.

---

## Step 5 — Implement the Drag Action Effect

**Files:**
* `main/profiles/<profile-name>/effects/DragEffect.hpp`
* `main/profiles/<profile-name>/effects/DragEffect.cpp`

For most profiles, reuse `main/profiles/inertial/effects/DragEffect.hpp/cpp`.

### Key Trigger Logic in `Test()`

The drag effect evaluates if the button is pressed and held for more than 500ms (`HELD` event) or released (`RELEASED` event) and manages transitions:

```cpp
bool DragEffect::Test(const Core::SaberDataPacket &packet) {
    if (!m_power.isActive()) {
        m_triggerMet = false;
        return m_active;
    }

    if (m_buttonId < Core::Platform::kMaxInputs) {
        const auto& input = packet.inputs[m_buttonId];
        using InputState = Core::InputDescriptor::State;

        if (input.current == InputState::HELD) {
            m_triggerMet = true;
        } else if (input.current == InputState::RELEASED) {
            m_triggerMet = false;
        }
    }

    return m_triggerMet || m_active;
}
```

### Key Execution Logic in `Run()`

The `Run()` method compares the pending state with the current state:
1. **Transition Inactive -> Active**: Selects a random looping drag sound file, plays it on loop (`loop = true`), and pushes the `BladeDragEffect` overlay.
2. **Transition Active -> Inactive**: Stops the loop audio, plays a random deactivation sound file (`loop = false`) from the `enddrag` folder, and starts the fade-out on the LED overlay.

---

## Step 6 — Register in the Profile

**File:** `main/profiles/<profile-name>/<ProfileName>.cpp`

Inside the `load()` method, register the `DragEffect` passing the target button ID (e.g. 0):

```cpp
bus.registerEffect(std::make_unique<Effects::DragEffect>(
    powerRef, audio, led, kDefinition, 0));
```

Populate the drag fields in the `InertialDefinition` literal:

```cpp
.fontDragCount    = <N>, // e.g. 1
.fontDragEndCount = <M>, // e.g. 4
.dragLedCount     = 8,   // 8 LEDs at the tip
```

---

## Step 7 — Verify the Build

```bash
idf.py build
```

Verify that there are no compilation errors or warnings.

---

## Step 8 — Manual Validation on Hardware

1. Flash the firmware.
2. Turn the saber ON (click the button once).
3. Press and hold the main button for more than 500ms → verify looping grinding audio starts and the tip of the blade glows incandescent orange/yellow.
4. Release the button → verify the grinding audio stops, a deactivation sound plays, and the orange tip fades out over 150ms.
