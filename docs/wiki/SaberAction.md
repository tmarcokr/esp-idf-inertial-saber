# Technical Specification: SaberAction System (The Action Bus)

## 1. System Overview
The **SaberAction System** is an asynchronous and modular event dispatcher designed for real-time data processing. Its main function is to act as an intermediate **Data Bus** between the information producers (IMU Sensors and Input Peripherals) and the event consumers (Audio and Light Engines).

The system allows the dynamic subscription of "Actions" to a constant **Data Stream**, where each action is responsible for evaluating its own trigger condition based on the global state of the system.

### 1.1. Architectural Placement
The SaberAction Bus is **application-level logic**, located under `main/core/`. It is not a reusable hardware component; it is the central nervous system that orchestrates the InertialSaber OS domain logic.

### 1.2. Platform Abstraction
The architecture targets the **ESP32-S3** exclusively (dual-core Xtensa with a hardware FPU and PSRAM).
The bus and the IMU adapter run on core 0. The audio pipeline (mixer, readers, `audio_ctrl`) and the background I/O tasks (`profile_ctrl`, `psram_loader`, `metrics`) run on core 1, so blocking audio and SD work never runs on the bus core. The LED render task is not pinned (see §5.3).

---

## 2. Data Stream Structure (`SaberDataPacket`)
The bus distributes a unified data packet in each update cycle. This packet contains the instantaneous state of all physical and interface descriptors of the saber. Every `InertialEffect` evaluated in the same cycle receives an **identical snapshot**, guaranteeing deterministic evaluation order.

### 2.1. Motion Descriptors (Kinetic Data)
See [Kinetic Metrics](KineticMetrics.md) for the definitions and the start-up behaviour.

| Attribute | Type | Description |
| :--- | :--- | :--- |
| `kineticEnergy` | `float` | Kinetic energy in G: magnitude of the linear acceleration with gravity removed. Values below `sensor.kinetic_deadband_g` are reported as 0. |
| `axisRotation` | `float[3]` | Angular velocity on X, Y, Z axes (°/s). Axes below `sensor.rotation_deadband_dps` are reported as 0. |
| `orientation` | `float` | Orientation vector: blade roll normalised to −1.0 … +1.0 (+1.0 tip straight up, 0 horizontal, −1.0 straight down). |
| `inertialOverload` | `float` | Accumulator charge level (0.0 to 1.0) based on sustained movement. |
| `inertialBurst` | `bool` | True for a single cycle when the Inertial Overload reaches 1.0. |

During the first **300 ms** after power-up (bus warm-up) the motion values are forced to rest (`kineticEnergy` = 0, `axisRotation` = 0, `orientation` = +1.0), so the first IMU FIFO drain cannot produce fake motion.

### 2.1.1. Timing Fields
Both timestamps come from `esp_timer` (microsecond clock since boot), not from the FreeRTOS tick.

| Attribute | Type | Description |
| :--- | :--- | :--- |
| `timestampMs` | `uint32_t` | `esp_timer` time in milliseconds at the start of this bus cycle. |
| `motionTimestampUs` | `int64_t` | `esp_timer` time in microseconds at which the IMU sample in this packet was read; `0` before the first sample. Several bus cycles can carry the same sample (timeout or input wakes); effects that must process each IMU sample once (e.g. clash detection) compare this value with the last one they saw. |

### 2.2. Interface Descriptors (Input Data)
The packet carries an **array of `InputDescriptor`** structs, indexed by input ID (Button 0...N, `kMaxInputs` = 4). This allows multiple physical buttons to be evaluated simultaneously by any effect. The initial deployment uses a single button (input ID 0); the array is sized for future expansion.

The input slots are cleared after every cycle: a gesture is seen by the effects in exactly one bus cycle.

#### `InputDescriptor` Structure
Each `InputDescriptor` contains a state-machine snapshot plus a **resolved gesture**, so effects do not have to time button presses themselves.

| Attribute | Type | Description |
| :--- | :--- | :--- |
| `current` | `State` enum | Current logical state: `Idle`, `Pressed`, `Held`, `Released`. |
| `previous` | `State` enum | State before the last transition. |
| `holdDurationMs` | `uint32_t` | On release: time the button was pressed. While held: `holdLevel` × 500 ms. |
| `lastTransitionMs` | `uint32_t` | Timestamp (`esp_timer`, ms) of the last press or release. |
| `gesture` | `Gesture` enum | `None`, `Click` or `HoldTick`. Set only in the event that resolves the gesture. |
| `pressCount` | `uint8_t` | With `gesture == Click`: number of clicks in the sequence (1, 2, 3, …). |
| `holdLevel` | `uint8_t` | With `gesture == HoldTick`: 1 at 500 ms, 2 at 1000 ms, and so on while the button stays pressed. |

#### Gesture Resolution (`InputAdapter`)
- **Click window: 400 ms.** Every press restarts it. When it expires with no new press, the pending presses are reported as one `Click` with `pressCount` = the number of presses.
- **Click resolved on release.** If the window expires while the button is still pressed, the click is not reported at that moment: it is reported when the button is released, unless a hold tick fires first. A press released between 400 and 500 ms therefore still counts as a click.
- **Hold: 500 ms.** If the button stays pressed for 500 ms, a `HoldTick` with `holdLevel` 1 is reported, then one more every 500 ms. The first hold tick **cancels every pending click** of the sequence, so a hold never produces a click.
- A click is reported 400 ms after the last press (or at the release if the press was longer).
- There is no software debounce in the adapter: the `GpioButton` component samples the pin every 10 ms.

#### Gestures Used by the Current Profile
| User action | Gesture | Effect | State required |
| :--- | :--- | :--- | :--- |
| 1 click | `Click`, `pressCount == 1` | Ignition (`PowerToggleEffect`) | Retracted |
| 1 click | `Click`, `pressCount == 1` | Blaster (`BlasterEffect`) | Ignited |
| 2 clicks | `Click`, `pressCount == 2` | Retraction (`PowerToggleEffect`) | Ignited |
| 3 clicks | `Click`, `pressCount == 3` | Next profile (`ProfileCycleEffect`) | Retracted or Faulted, no switch pending |
| Hold ≥ 500 ms | `HoldTick`, `holdLevel == 1` | Drag starts (`DragEffect`); it ends at the release (`current == Released`) | Ignited |

Two consecutive blasters must be pressed **more than 400 ms apart**: two presses inside one click window resolve as a double click, which retracts the blade. Clicks during the ignition or retraction sweep, and any gesture while Locked or Switching, are ignored.

#### Input Injection Model
The bus does **not** poll hardware directly. Button state is injected externally via a thread-safe queue:

```text
[ GpioButton ]  ──callback──►  [ InputAdapter ]  ──queue──►  [ SaberAction Bus ]
  (10 ms poll)                   (State machine,               (Drains into packet)
                                  click/hold timers)
```

The `InputAdapter` owns the state machine logic (press counting within the click window, hold ticks, transition tracking). Its click and hold timers run on the `esp_timer` task. The bus simply copies the latest `InputDescriptor` of each input into the packet each cycle (input queue depth 8; a full queue drops the event and counts it in `input_events_dropped`). This decouples the effects from hardware — a BLE remote, serial debug command, or physical button all produce identical `InputDescriptor` data.

---

## 3. Typology of Actions (The InertialEffect Interface)
All actions must implement the `InertialEffect` interface (`main/core/InertialEffect.hpp`), which allows them to be evaluated by the bus:

- `test(const SaberDataPacket&)` — returns true if the effect must run in this cycle.
- `run()` — executes the response (audio commands, LED overlays, state transitions).
- `priority()` — fixed at construction; sets the evaluation order (§4.1).

Three logical categories are defined according to their behavior:

### 3.1. Flow Modulators (Continuous Actions)
Actions that operate persistently (Priority 0). They do not wait for a trigger event but transform stream data into output parameters (e.g., `InertialSwingEffect` mapping kinetic energy to volume).

### 3.2. Event Triggers (Discrete Actions)
Actions with a single activation signature. They implement the `test()` method to evaluate if an exact descriptor match is met (e.g., a kinetic impact or a blaster click).

### 3.3. Pattern Detectors (Sequential Actions)
Actions that maintain an internal buffer of previous states. They are activated after detecting a chronological sequence of changes in the descriptors within a defined time window (e.g., the clash detector keeps the last 15 ms of IMU samples).

---

## 4. Hierarchy and Conflict Management (Priority System)
Each `InertialEffect` carries a priority value. The values used by the current profile are:

| Priority | Effects |
| :--- | :--- |
| **0** | `PreloadWaitEffect`, `InertialSwingEffect`, `InertialLightEffect` (flow modulators and the preload lock) |
| **1** | `PowerToggleEffect`, `DragEffect`, `ProfileCycleEffect` |
| **2** | `BlasterEffect`, `KineticImpactEffect` (Clash) |

> [!NOTE]
> The priority is **not** an arbitration level. The bus does not preempt, suspend, attenuate or skip an effect because of its priority. Its only runtime meaning is the evaluation order (§4.1). Conflicts between effects are avoided by their trigger conditions (for example, the power state each effect requires).

### 4.1. Evaluation Order
The only runtime meaning of the priority is the **evaluation order within a bus cycle**:
- Effects are evaluated in ascending priority order: priority `0` first.
- Effects with the same priority are evaluated in **registration order**, i.e. the order of the `EffectSet::add()` calls in `ConfigurableProfile::buildEffects()`. `add()` inserts each new effect after all effects with a lower or equal priority, so this order is guaranteed.
- An effect set holds at most 16 effects (`EffectSet::kMaxEffects`).

Resulting order for the current profile:

| # | Effect | Priority |
| :--- | :--- | :--- |
| 1 | `PreloadWaitEffect` | 0 |
| 2 | `InertialSwingEffect` | 0 |
| 3 | `InertialLightEffect` | 0 |
| 4 | `PowerToggleEffect` | 1 |
| 5 | `DragEffect` | 1 |
| 6 | `ProfileCycleEffect` | 1 |
| 7 | `BlasterEffect` | 2 |
| 8 | `KineticImpactEffect` (Clash) | 2 |

### 4.2. Ducking Ownership
Priority-based attenuation (ducking) is **not managed by the bus**. Each consumer engine (InertialSwing, InertialLight) would be responsible for its own ducking logic. The bus only provides the priority metadata; rendering decisions are decentralized.

> [!NOTE]
> Priority-based ducking is **not implemented** in either engine. (The hum ducking of InertialSwing depends on the swing intensity, not on effect priority.)

---

## 5. Threading Model (Hybrid Event-Driven)
The bus task uses a **hybrid event-driven model** with timeout fallback to balance responsiveness with continuous evaluation.

### 5.1. Task Architecture

```text
┌─────────────────────────────────────────────────────────────────┐
│                     SaberAction Bus Task                        │
│                                                                 │
│  ┌───────────────┐   ┌────────────────────┐   ┌──────────────┐  │
│  │ Block on      │──►│ Commit staged      │──►│ Load staged  │  │
│  │ notification  │   │ effect set (if any)│   │ motion +     │  │
│  │ (timeout 10ms)│   │ (pointer swap)     │   │ filters      │  │
│  └───────────────┘   └────────────────────┘   └──────┬───────┘  │
│                                                      │          │
│                          ┌───────────────────────────▼───────┐  │
│                          │ Inertial Overload update          │  │
│                          │ Drain input queue into the packet │  │
│                          └───────────────────────────┬───────┘  │
│                                                      │          │
│                          ┌───────────────────────────▼───────┐  │
│                          │ for (effect : active effect set)  │  │
│                          │   if (effect->test(packet))       │  │
│                          │     effect->run()                 │  │
│                          └───────────────────────────────────┘  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2. Wake Sources
| Source | Mechanism | Purpose |
| :--- | :--- | :--- |
| **IMU DMP** | Task notification from the IMU adapter task (`updateMotion()`) | New motion data available (100 Hz, measured on the reference board) |
| **InputAdapter** | Queue push + task notification | Button state transition or resolved gesture |
| **Timeout (10 ms)** | FreeRTOS notification timeout (fallback) | Ensures continuous evaluation for Flow Modulators during calm periods |

> [!NOTE]
> **Measured on the reference board (debug build, 2026-10-02):** IMU sample rate 100 Hz (99–101 Hz per 1 s window); bus rate about 155 Hz on average (151–171 Hz per session, 128–185 Hz per 1 s window), with about 35–40 % of the cycles woken by the timeout; bus cycle 70–240 µs on average, max 1.47 ms while ignited (budget 2 ms); motion age about 3.3–3.9 ms on average, max about 10.9 ms; one-shot audio latency 12–18 ms on average, max 25.5 ms. At 100 Hz the 15 ms clash window spans 1–2 IMU samples. See [Diagnostics](Diagnostics.md).

### 5.3. Core Affinity and Task Map
Stack size, priority and core of every task are defined in one table, `TaskTable` in `main/system/hardware/HardwareConfig.hpp`. Tasks owned by `main/` are created from it with `xTaskCreatePinnedToCore`. Tasks created by components or by ESP-IDF with fixed parameters are listed as **reference only**: the table documents them and feeds the metrics report ([Diagnostics](Diagnostics.md)), but does not apply them.

| Task | Owner | Stack (B) | Priority | Core | Applied from the table |
| :--- | :--- | ---: | ---: | :---: | :--- |
| `esp_timer` | ESP-IDF | `CONFIG_ESP_TIMER_TASK_STACK_SIZE` + 512 | 22 | 0 | No (sdkconfig) |
| `imu_adapter` | `ImuAdapter` | 4096 | 9 | 0 | Yes |
| `saber_bus` | `SaberActionBus` | 8192 | 8 | 0 | Yes |
| `main` | ESP-IDF (boot only) | `CONFIG_ESP_MAIN_TASK_STACK_SIZE` + 512 | 1 | 0 | No (sdkconfig) |
| `audio_mixer` | `AudioEngine` | 4096 | 10 | 1 | No (component) |
| `audio_mem_reade` | `AudioEngine` (created as `audio_mem_reader`; FreeRTOS truncates the name) | 4096 | 9 | 1 | No (component) |
| `audio_ctrl` | `AudioController` | 4096 | 7 | 1 | Yes |
| `audio_sd_reader` | `AudioEngine` | 8192 | 6 | 1 | No (component) |
| `psram_loader` | `PsramAudioCache` | 4096 | 2 | 1 | Yes |
| `profile_ctrl` | `ProfileManager` (builds the next profile, writes the active index) | 6144 | 3 | 1 | Yes |
| `metrics` | `MetricsReporter` (metrics builds only) | 4096 | 1 | 1 | Yes |
| `SmartLedTask` | `SmartLed::Engine` | 4096 | 5 | any | Stack and priority only (the component API takes no core) |
| `gpio_btn_tsk` | `GpioButton` | 4096 | 5 | any | No (component) |

Core and priority policy:
- **Core 0 — motion and decision path:** IMU adapter (9) > bus (8). The IMU adapter must stay above the bus on the same core (enforced by a `static_assert`). The `esp_timer` task (22) runs the input adapter's click and hold timers.
- **Core 1 — audio pipeline and background I/O:** mixer (10) > PSRAM reader (9) > `audio_ctrl` (7) > SD reader (6) > `profile_ctrl` (3) > PSRAM loader (2) > metrics reporter (1). The effects on the bus never call the audio engine directly: they queue commands to `audio_ctrl`, which runs the blocking `play()`/`playLinked()`/`stop()` calls on core 1. `profile_ctrl` must stay on the other core than the bus (enforced by a `static_assert`), so its allocations are never counted against the bus.
- **Unpinned:** the LED render task and the button poll task run at priority 5 on either core. On core 0 they cannot preempt the bus or the IMU adapter; on core 1 they cannot preempt any audio task with priority ≥ 6. Pinning them needs a component change.

---

## 6. Power States and Dynamic Profile Management

### 6.1. Power State Machine
Each effect set owns one `PowerStateMachine` (`main/profiles/PowerStateMachine.cpp`). Only the bus task changes its state (or the building task, before the set is handed to the bus); other tasks may read it.

| State | Meaning | Left by |
| :--- | :--- | :--- |
| **Locked** | Initial state: the profile's `hum.wav` is being copied to PSRAM. Ignition is blocked; status LED blinking yellow. | `PreloadDone` → Retracted (plays `font.wav`); `PreloadFailed` → Faulted |
| **Retracted** | Ready, blade off. | 1 click → Igniting; 3 clicks → Switching |
| **Igniting** | Ignition sweep (`blade_timings.ignition_duration_ms`). | Sweep end → Ignited |
| **Ignited** | Blade on; swing, light, blaster, clash and drag active. | 2 clicks → Retracting |
| **Retracting** | Retraction sweep (`blade_timings.retraction_duration_ms`). | Sweep end → Retracted |
| **Faulted** | The hum copy failed. Ignition disabled; status LED solid red. | 3 clicks → Switching |
| **Switching** | A profile switch is in progress. Ignition blocked. Terminal: the set is retired from this state. | — |

A transition that is not in this table is rejected and logged as a warning.

### 6.2. Off-Bus Profile Switch
The system swaps the active list of actions at runtime without pausing or loading the bus.

1. **Request** (bus): on a triple click, `ProfileCycleEffect` moves the active set to `Switching` and calls `ProfileManager::requestNext()`, which only sets an atomic flag and notifies `profile_ctrl` (no allocation, O(1)).
2. **Build** (`profile_ctrl`, core 1): the next profile's effect set is built with a fresh state machine in `Locked`, and the copy of its `hum.wav` is requested. The set is handed to the bus with `stageEffects()` (one atomic slot).
3. **Commit** (bus): at the top of the next cycle the bus swaps the staged set in with a pointer exchange (O(1), no allocation; metric `profile_commit`), applies its physics (`overload` and `sensor` sections) and publishes the old set as retired.
4. **Cleanup** (`profile_ctrl`): the old set is taken back and destroyed off the bus. No effect is ever destroyed on the bus task.
5. **Unlock** (bus): the new set's `PreloadWaitEffect` unlocks ignition when its hum copy completes, as at boot. The active index is saved about 1.5 s later, never while the blade is lit (see [Profiles](Profiles.md) §6).

Further triple clicks are ignored while a switch is pending. The full boot and switch sequence, the PSRAM budget and the persistence rules are in [Profiles](Profiles.md) §5–§6.

---

## 7. Execution Logic (Abstract Pseudocode)

```cpp
// Base contract for any action in the system
class InertialEffect {
public:
    explicit InertialEffect(uint8_t priority);
    uint8_t priority() const;                              // evaluation order
    virtual bool test(const SaberDataPacket& packet) = 0;  // Evaluation
    virtual void run() = 0;                                // Rendering
};

// Producers (other tasks)
ImuAdapter:   bus.updateMotion(sample);         // stage the newest sample, notify the bus
InputAdapter: bus.pushInputEvent(id, descriptor); // queue the event, notify the bus

// Bus task loop (Hybrid Event-Driven)
while (running) {
    waitForNotification(10 ms);                 // IMU, input or timeout

    commitStagedEffects();                      // profile switch: O(1) pointer swap
    packet.timestampMs = now();
    applyStagedMotion(packet);                  // copy staged sample, warm-up, deadbands, orientation
    computeInertialOverload(packet);
    drainInputQueue(packet);

    for (auto& effect : activeEffects) {
        if (effect->test(packet)) {
            effect->run();                      // queues audio commands and LED overlays
        }
    }

    packet.inputs = {};                         // gestures are seen in one cycle only
}
```

Effects never block: audio playback is queued to `audio_ctrl`, and the LED engine renders on its own task.

---

## 8. System Integration Layer (`SaberSystem`)

The `SaberSystem` is the top-level orchestrator that bridges the **hardware layer** (components) with the **application layer** (SaberAction Bus). It owns all peripheral instances, spawns the adapter tasks, and provides a single `start()` entry point to the OS. This ensures that the application entry point (`app_main`) remains minimal and that all initialization sequencing is centralized.

### 8.1. Integration Architecture

The SaberAction Bus does not interact with hardware directly. Two **adapter layers** translate raw peripheral data into bus-compatible formats:

```text
┌──────────────┐                              ┌──────────────────────┐
│   Mpu6050    │                              │                      │
│   (I2C/DMP)  │──readData()──► IMU Adapter ──► bus.updateMotion()   │
│              │               (Computes       │                      │
│   SDA/SCL/INT│                KineticEnergy,  │   SaberAction Bus    │
└──────────────┘                AxisRotation,   │                      │
                                Orientation)    │   ┌──────────────┐   │
                                               │   │ SaberData    │   │
┌──────────────┐                               │   │ Packet       │   │
│  GpioButton  │                               │   │ (snapshot)   │   │
│  (GPIO Poll) │──callback──► Input Adapter ──► bus.pushInputEvent()  │
│              │              (Tracks state,    │   └──────┬───────┘   │
│  Button 0..N │               resolves gestures)│          │           │
└──────────────┘                                │          ▼           │
                                               │   test() → run()    │
                                               │   (InertialEffects)  │
                                               └──────────────────────┘
                                                          │
                                                ┌─────────┴──────────┐
                                                ▼                    ▼
                                        InertialSwing         InertialLight
                                        (Audio Engine)        (Visual Engine)
```

### 8.2. IMU Adapter (Kinetic Parser)

The IMU Adapter is a dedicated FreeRTOS task that reads DMP-processed data from the MPU-6050 FIFO and transforms it into the three kinetic descriptors consumed by the bus:

| Raw DMP Output | Transformation | Bus Field |
| :--- | :--- | :--- |
| Linear acceleration (x, y, z) | `sqrt(x² + y² + z²)` — gravity removed by the DMP | `kineticEnergy` (G) |
| Raw gyroscope (x, y, z) | Direct passthrough (°/s) | `axisRotation[3]` |
| Quaternion → Euler **roll** | Radians → degrees; the bus clamps to ±90° and divides by 90 | `orientation` (−1.0 … +1.0) |

**Start-up fallback.** Until the DMP gravity estimate has converged, the adapter publishes values derived from the raw accelerometer instead: kinetic energy = `| |a| − 1 g |` and roll from the accelerometer tilt. The switch to the DMP values happens once, on a quasi-static sample after the DMP gravity has matched the accelerometer within 4° for 200 ms (20 samples), with a 20 s ceiling. Details in [Kinetic Metrics](KineticMetrics.md) §5.

The adapter runs at a priority **above** the bus task to guarantee that fresh motion data is always available before the bus evaluates its effects. The IMU interrupt (data ready) wakes it; without an interrupt it polls every 20 ms. When new data is processed, the adapter calls `updateMotion()`, which stages the values and wakes the bus via FreeRTOS task notification.

### 8.3. Input Adapter (Button State Machine)

Each physical button is assigned a unique **Input ID** (0...N) and connected to the bus via a thin state-machine adapter. The adapter translates raw hardware callbacks into `InputDescriptor` snapshots:

| Source | State Machine Action | Bus Injection |
| :--- | :--- | :--- |
| `PressDown` callback | `previous ← current`, `current ← Pressed`, pending clicks +1, click window (400 ms) restarted | `pushInputEvent()` (no gesture) |
| `PressUp` callback | `previous ← current`, `current ← Released`, `holdDurationMs` = press time, hold ticks stopped | `pushInputEvent()` (no gesture); then the pending `Click` if it was deferred to the release |
| Click window expiry (`esp_timer`) | Button released: report the pending clicks. Button still pressed: defer the click to the release | `pushInputEvent()` with `Click` |
| `LongPress(500 ms)` callback, then a 500 ms periodic timer | Pending clicks cancelled, `current ← Held`, `holdLevel` +1 | `pushInputEvent()` with `HoldTick` |

This architecture guarantees **complete hardware decoupling**: the same `InputDescriptor` format can be produced by a physical GPIO button, a BLE remote, or a serial debug command. The `InertialEffect` evaluation logic is identical regardless of the input source.

### 8.4. Adding New Input Peripherals

To register a new button (e.g., an auxiliary button on Input ID 1):

1. Instantiate the hardware component (`GpioButton` on the target GPIO; the pin needs a hardware review).
2. Create a new `InputDescriptor` tracking variable for its state.
3. Bind its callbacks to the same state-machine pattern, using the new Input ID.
4. No changes are required in the bus, the packet structure, or any existing `InertialEffect`.

Effects that respond to the new button simply evaluate `packet.inputs[1]` in their `test()` method.
