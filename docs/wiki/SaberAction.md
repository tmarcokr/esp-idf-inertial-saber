# Technical Specification: SaberAction System (The Action Bus)

## 1. System Overview
The **SaberAction System** is an asynchronous and modular event dispatcher designed for real-time data processing. Its main function is to act as an intermediate **Data Bus** between the information producers (IMU Sensors and Input Peripherals) and the event consumers (Audio and Light Engines).

The system allows the dynamic subscription of "Actions" to a constant **Data Stream**, where each action is responsible for evaluating its own trigger condition based on the global state of the system.

### 1.1. Architectural Placement
The SaberAction Bus is **application-level logic**, located under `main/core/`. It is not a reusable hardware component; it is the central nervous system that orchestrates the InertialSaber OS domain logic.

### 1.2. Platform Abstraction
The bus code is 100% portable between ESP32-S3 and ESP32-C6. Platform-specific constants (core affinity, task priorities) are centralized in `PlatformConfig.hpp` and resolved at compile time via `CONFIG_IDF_TARGET_*` macros provided by ESP-IDF.

| Aspect | ESP32-C6 (RISC-V) | ESP32-S3 (Xtensa) |
| :--- | :--- | :--- |
| **Cores** | Single-core (all tasks on core 0) | Dual-core (bus on core 0, engines on core 1) |
| **FPU** | Software-emulated | Hardware FPU |

---

## 2. Data Stream Structure (`SaberDataPacket`)
The bus distributes a unified data packet in each update cycle. This packet contains the instantaneous state of all physical and interface descriptors of the saber. Every `InertialEffect` evaluated in the same cycle receives an **identical snapshot**, guaranteeing deterministic evaluation order.

### 2.1. Motion Descriptors (Kinetic Data)
| Attribute | Type | Description |
| :--- | :--- | :--- |
| `KineticEnergy` | `float` | Total G-Force calculated by the inertia engine. |
| `AxisRotation` | `float[3]` | Angular velocity on X, Y, Z axes (deg/s). |
| `OrientationVector` | `float` | Blade inclination angle relative to gravity. |

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
| `lastTransition_ms` | `uint32_t` | Timestamp (system tick) of the last state change. Used by Pattern Detector effects. |

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

### 4.1. Ducking Ownership
Priority-based attenuation (ducking) is **not managed by the bus**. Each consumer engine (InertialSwing, InertialLight) is responsible for implementing its own ducking logic when it receives commands from effects of different priority levels. The bus only provides the priority metadata; rendering decisions are decentralized.

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
│  │ (timeout ~2ms)│   │               │   │ (InputDescriptor)│   │
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
| **Timeout (~2ms)** | FreeRTOS notification timeout | Ensures continuous evaluation for Flow Modulators during calm periods |

### 5.3. Core Affinity

| Platform | Bus Task Core | Engine Tasks Core |
| :--- | :--- | :--- |
| **ESP32-C6** | Core 0 (only core) | Core 0 (shared) |
| **ESP32-S3** | Core 0 (PRO) | Core 1 (APP) |

Task creation uses `xTaskCreatePinnedToCore` with platform-configurable core ID constants from `PlatformConfig.hpp`.

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
    // Block until notification or timeout (~2ms)
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