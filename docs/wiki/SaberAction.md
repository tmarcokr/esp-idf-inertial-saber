# Technical Specification: SaberAction System (The Action Bus)

## 1. System Overview
The **SaberAction System** is an asynchronous and modular event dispatcher designed for real-time data processing. Its main function is to act as an intermediate **Data Bus** between the information producers (IMU Sensors and Input Peripherals) and the event consumers (Audio and Light Engines).

The system allows the dynamic subscription of "Actions" to a constant **Data Stream**, where each action is responsible for evaluating its own trigger condition based on the global state of the system.

### 1.1. Architectural Placement
The SaberAction Bus is **application-level logic**, located under `main/core/`. It is not a reusable hardware component; it is the central nervous system that orchestrates the InertialSaber OS domain logic.

### 1.2. Platform Abstraction
La arquitectura está optimizada exclusivamente para **ESP32-S3** (Xtensa Dual-Core con soporte FPU por hardware y PSRAM). 
El bus se ejecuta en el Core 0 (PRO) y los motores de renderizado en el Core 1 (APP), garantizando latencia cero.

---

## 2. Data Stream Structure (`SaberDataPacket`)
The bus distributes a unified data packet in each update cycle. This packet contains the instantaneous state of all physical and interface descriptors of the saber. Every `InertialEffect` evaluated in the same cycle receives an **identical snapshot**, guaranteeing deterministic evaluation order.

### 2.1. Motion Descriptors (Kinetic Data)
| Attribute | Type | Description |
| :--- | :--- | :--- |
| `KineticEnergy` | `float` | Total G-Force calculated by the inertia engine. |
| `AxisRotation` | `float[3]` | Angular velocity on X, Y, Z axes (deg/s). |
| `OrientationVector` | `float` | Blade inclination angle relative to gravity. |
| `InertialOverload` | `float` | Accumulator charge level (0.0 to 1.0) based on sustained movement. |
| `InertialBurst` | `bool` | True for a single cycle when InertialOverload hits 1.0. |

### 2.1.1. Timing Fields
Both timestamps come from `esp_timer` (microsecond clock since boot), not from the FreeRTOS tick.

| Attribute | Type | Description |
| :--- | :--- | :--- |
| `timestampMs` | `uint32_t` | `esp_timer` time in milliseconds at the start of this bus cycle. |
| `motionTimestampUs` | `int64_t` | `esp_timer` time in microseconds at which the IMU sample in this packet was read; `0` before the first sample. Several bus cycles can carry the same sample (timeout or input wakes); effects that must process each IMU sample once (e.g. clash detection) compare this value with the last one they saw. |

### 2.2. Interface Descriptors (Input Data)
The packet carries an **array of `InputDescriptor`** structs, indexed by input ID (Button 0...N). This allows multiple physical buttons to be evaluated simultaneously by any effect. The initial deployment uses a single button; the array is sized for future expansion (2-3 buttons).

#### `InputDescriptor` Structure
Each `InputDescriptor` contains a full state-machine snapshot, providing effects with enough context to discriminate between complex interaction patterns (single click, double-click, long press, click-then-hold, etc.) from a single physical button.

| Attribute | Type | Description |
| :--- | :--- | :--- |
| `current` | `State` enum | Current logical state: `IDLE`, `PRESSED`, `HELD`, `RELEASED`. |
| `previous` | `State` enum | State in the previous evaluation cycle. Enables transition detection (e.g., `PRESSED → HELD`). |
| `holdDuration_ms` | `uint32_t` | Continuous hold time in milliseconds. Resets to 0 when state returns to `IDLE`. |
| `pressCount` | `uint8_t` | Rapid press counter within a configurable time window (e.g., 400ms). Resets to 0 after the window expires with no new press. |
| `lastTransition_ms` | `uint32_t` | Timestamp (`esp_timer`, ms) of the last state change. Used by Pattern Detector effects. |

#### Example Trigger Patterns
| User Action | Effect Evaluation Logic |
| :--- | :--- |
| **Power toggle** | `pressCount == 1 && current == RELEASED && holdDuration_ms < 300` |
| **Profile switch** | `pressCount == 2 && current == RELEASED` |
| **Force effect** | `current == HELD && holdDuration_ms > 800` |
| **Lockup hold** | `previous == PRESSED && current == HELD && holdDuration_ms > 500` (combined with motion check) |

#### Input Injection Model
The bus does **not** poll hardware directly. Button state is injected externally via a thread-safe queue:

```text
[ GpioButton ]  ──callback──►  [ InputAdapter ]  ──queue──►  [ SaberAction Bus ]
                                 (State Machine)               (Drains into packet)
                                 (Debounce, Count)
```

The `InputAdapter` owns the state machine logic (debouncing, press counting within time windows, hold tracking, transition detection). The bus simply snapshots the latest `InputDescriptor` into the packet each cycle. This decouples the effects from hardware — a BLE remote, serial debug command, or physical button all produce identical `InputDescriptor` data.

---

## 3. Typology of Actions (The InertialEffect Interface)
All actions must implement the `InertialEffect` interface, which allows them to be evaluated by the bus. Three logical categories are defined according to their behavior:

### 3.1. Flow Modulators (Continuous Actions)
Actions that operate persistently (Priority 0). They do not wait for a trigger event but transform stream data into output parameters (e.g., `InertialSwing` mapping G-force to volume).

### 3.2. Event Triggers (Discrete Actions)
Actions with a single activation signature. They implement the `Test()` method to evaluate if an exact descriptor match is met (e.g., a kinetic impact or a deflection burst).

### 3.3. Pattern Detectors (Sequential Actions)
Actions that maintain an internal buffer of previous states. They are activated after detecting a chronological sequence of changes in the descriptors within a defined time window.

---

## 4. Hierarchy and Conflict Management (Priority System)
To manage the interaction between multiple actions operating simultaneously on the same resources (audio channels or LEDs), the system implements a priority table within each `InertialEffect`.

| Level | Category | Behavior |
| :--- | :--- | :--- |
| **0** | **Background** | Persistent actions (Hum/Swing). Can be attenuated (ducking). |
| **1** | **Standard** | Normal priority events (Deflection Burst). Mixed additively. |
| **2** | **Override** | High priority events (Kinetic Impact). Cause forced attenuation in lower levels. |
| **3** | **System** | Critical hardware events (Power On/Off). Total control over the bus. |

> [!NOTE]
> The category names (Background, Standard, Override, System) are **labels only**. The bus does not preempt, suspend or skip an effect because of its priority, and the attenuation described in the *Behavior* column is not implemented yet (see §4.2).

### 4.1. Evaluation Order
The only runtime meaning of the priority is the **evaluation order within a bus cycle**:
- Effects are evaluated in ascending priority order: priority `0` first.
- Effects with the same priority are evaluated in **registration order**, i.e. the order of the `registerEffect()` calls in `ConfigurableProfile::load()`. The bus inserts each new effect after all effects with a lower or equal priority, so this order is guaranteed.
- If an effect's `run()` replaces the registered effects (a profile swap), the rest of the cycle is skipped.

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
Priority-based attenuation (ducking) is **not managed by the bus**. Each consumer engine (InertialSwing, InertialLight) is responsible for implementing its own ducking logic when it receives commands from effects of different priority levels. The bus only provides the priority metadata; rendering decisions are decentralized.

> [!NOTE]
> Priority-based ducking is **not implemented yet** in either engine. (The hum ducking of InertialSwing depends on the swing intensity, not on effect priority.)

---

## 5. Threading Model (Hybrid Event-Driven)
The bus task uses a **hybrid event-driven model** with timeout fallback to balance responsiveness with continuous evaluation.

### 5.1. Task Architecture

```text
┌─────────────────────────────────────────────────────────────────┐
│                     SaberAction Bus Task                        │
│                                                                 │
│  ┌──────────────┐   ┌───────────────┐   ┌──────────────────┐   │
│  │ Block on      │──►│ Drain IMU     │──►│ Drain Input      │   │
│  │ Notification  │   │ (MotionData)  │   │ Queue            │   │
│  │ (timeout 10ms)│   │               │   │ (InputDescriptor)│   │
│  └──────────────┘   └───────────────┘   └──────────────────┘   │
│                                                │                │
│                          ┌─────────────────────▼──────────┐     │
│                          │ Build SaberDataPacket          │     │
│                          │ (Motion + Input snapshot)      │     │
│                          └─────────────────────┬──────────┘     │
│                                                │                │
│                          ┌─────────────────────▼──────────┐     │
│                          │ for (effect : activeEffects)   │     │
│                          │   if (effect->Test(packet))    │     │
│                          │     effect->Run()              │     │
│                          └────────────────────────────────┘     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2. Wake Sources
| Source | Mechanism | Purpose |
| :--- | :--- | :--- |
| **IMU DMP** | Task notification from ISR/reader task | New motion data available (~200Hz) |
| **InputAdapter** | Queue push + task notification | Button state transition detected |
| **Timeout (10 ms)** | FreeRTOS notification timeout (fallback) | Ensures continuous evaluation for Flow Modulators during calm periods |

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
| `profile_store` | `ActiveProfileStore` | 3072 | 1 | 1 | Yes |
| `metrics` | `MetricsReporter` (metrics builds only) | 4096 | 1 | 1 | Yes |
| `SmartLedTask` | `SmartLed::Engine` | 4096 | 5 | any | Stack and priority only (the component API takes no core) |
| `gpio_btn_tsk` | `GpioButton` | 4096 | 5 | any | No (component) |

Core and priority policy:
- **Core 0 — motion and decision path:** IMU adapter (9) > bus (8). The IMU adapter must stay above the bus on the same core (enforced by a `static_assert`). The `esp_timer` task (22) runs the input adapter's click and hold timers.
- **Core 1 — audio pipeline and background I/O:** mixer (10) > PSRAM reader (9) > `audio_ctrl` (7) > SD reader (6) > PSRAM loader (2) > profile store (1) = metrics reporter (1). The effects on the bus never call the audio engine directly: they queue commands to `audio_ctrl`, which runs the blocking `play()`/`stop()` calls on core 1.
- **Unpinned:** the LED render task and the button poll task run at priority 5 on either core. On core 0 they cannot preempt the bus or the IMU adapter; on core 1 they cannot preempt any audio task with priority ≥ 6. Pinning them needs a component change.

---

## 6. Dynamic Profile Management (InertialProfile)
The system allows swapping the active list of actions at runtime.

1. **Profile Loading:** The `InertialProfile` injects its `InertialDefinition` into the core engines and registers its `InertialEffect` objects on the bus.
2. **Cleanup:** The bus ensures proper memory release of actions from the previous profile.
3. **Execution:** The main loop iterates over the active list, passing the `SaberDataPacket` reference to the `Test()` method of each effect.

---

## 7. Execution Logic (Abstract Pseudocode)

```cpp
// Base contract for any action in the system
class InertialEffect {
public:
    uint8_t Priority;
    virtual bool Test(const SaberDataPacket& packet) = 0; // Evaluation
    virtual void Run() = 0;                               // Rendering
};

// Main processing loop (Hybrid Event-Driven)
while (system_active) {
    // Block until notification or timeout (10 ms fallback)
    waitForEvent(timeout_ms);

    // Build unified snapshot
    SaberDataPacket currentPacket;
    currentPacket.motion = ImuAdapter::getLatestMotionData();
    currentPacket.inputs = InputAdapter::getSnapshot();

    // Evaluate all registered effects
    for (auto& effect : activeProfile.effects) {
        if (effect->Test(currentPacket)) {
            effect->Run();
        }
    }

    OutputRenderer::flush();
}
```

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
│  Button 0..N │               hold duration,   │          │           │
└──────────────┘               press count)     │          ▼           │
                                               │   Test() → Run()    │
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
| Linear Acceleration (x, y, z) | `sqrt(x² + y² + z²)` — gravity subtracted by DMP | `KineticEnergy` (G-Force) |
| Raw Gyroscope (x, y, z) | Direct passthrough (deg/s) | `AxisRotation[3]` |
| Quaternion → Euler Pitch | `atan2(gravity)` → radians → degrees | `OrientationVector` (Blade Angle) |

The adapter runs at a priority **above** the bus task to guarantee that fresh motion data is always available before the bus evaluates its effects. When new data is processed, the adapter calls `updateMotion()`, which stages the values and wakes the bus via FreeRTOS task notification.

### 8.3. Input Adapter (Button State Machine)

Each physical button is assigned a unique **Input ID** (0...N) and connected to the bus via a thin state-machine adapter. The adapter translates raw hardware callbacks into `InputDescriptor` snapshots:

| Hardware Callback | State Machine Action | Bus Injection |
| :--- | :--- | :--- |
| `PressDown` | `previous ← current`, `current ← PRESSED`, `pressCount++` | `pushInputEvent(id, descriptor)` |
| `PressUp` | `previous ← current`, `current ← RELEASED`, compute `holdDuration_ms` | `pushInputEvent(id, descriptor)` |
| `LongPress(N ms)` | `current ← HELD`, `holdDuration_ms = N` | `pushInputEvent(id, descriptor)` |

This architecture guarantees **complete hardware decoupling**: the same `InputDescriptor` format can be produced by a physical GPIO button, a BLE remote, or a serial debug command. The `InertialEffect` evaluation logic is identical regardless of the input source.

### 8.4. Adding New Input Peripherals

To register a new button (e.g., an auxiliary button on Input ID 1):

1. Instantiate the hardware component (`GpioButton` on the target GPIO).
2. Create a new `InputDescriptor` tracking variable for its state.
3. Bind its callbacks to the same state-machine pattern, using the new Input ID.
4. No changes are required in the bus, the packet structure, or any existing `InertialEffect`.

Effects that respond to the new button simply evaluate `packet.inputs[1]` in their `Test()` method.