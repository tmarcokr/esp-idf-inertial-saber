# Technical Specification: SaberAction System (The Action Bus)

## 1. System Overview
The **SaberAction System** is an asynchronous and modular event dispatcher designed for real-time data processing. Its main function is to act as an intermediate **Data Bus** between the information producers (IMU Sensors and Input Peripherals) and the event consumers (Audio and Light Engines).

The system allows the dynamic subscription of "Actions" to a constant **Data Stream**, where each action is responsible for evaluating its own trigger condition based on the global state of the system.

---

## 2. Data Stream Structure (`SaberDataPacket`)
The bus distributes a unified data packet in each update cycle. This packet contains the instantaneous state of all physical and interface descriptors of the saber.

### 2.1. Motion Descriptors (Kinetic Data)
| Attribute | Type | Description |
| :--- | :--- | :--- |
| `KineticEnergy` | `float` | Total G-Force calculated by the inertia engine. |
| `AxisRotation` | `float[3]` | Angular velocity on X, Y, Z axes (deg/s). |
| `OrientationVector` | `float` | Blade inclination angle relative to gravity. |

### 2.2. Interface Descriptors (Input Data)
| Attribute | Type | Description |
| :--- | :--- | :--- |
| `InputID` | `uint8_t` | Input peripheral identifier (Button 0...N). |
| `InputState` | `enum` | Logical state: `IDLE`, `ACTIVE`, `HOLD`, `RELEASED`. |
| `TriggerDuration` | `uint32_t` | Elapsed time (ms) in the current state. |
| `InputCounter` | `uint8_t` | Record of rapid activations (multi-click). |

---

## 3. Typology of Actions (The InertialEffect Interface)
All actions must implement the `InertialEffect` interface, which allows them to be evaluated by the bus. Three logical categories are defined according to their behavior:

### 3.1. Flow Modulators (Continuous Actions)
Actions that operate persistently (Priority 0). They do not wait for a trigger event but transform stream data into output parameters (e.g., `InertialSwing` mapping G-force to volume).

### 3.2. Event Triggers (Discrete Actions)
Actions with a single activation signature. They implement the `Test()` method to evaluate if an exact descriptor match is met (e.g., a clash or a blaster deflection).

### 3.3. Pattern Detectors (Sequential Actions)
Actions that maintain an internal buffer of previous states. They are activated after detecting a chronological sequence of changes in the descriptors within a defined time window.

---

## 4. Hierarchy and Conflict Management (Priority System)
To manage the interaction between multiple actions operating simultaneously on the same resources (audio channels or LEDs), the system implements a priority table within each `InertialEffect`.

| Level | Category | Behavior |
| :--- | :--- | :--- |
| **0** | **Background** | Persistent actions (Hum/Swing). Can be attenuated (ducking). |
| **1** | **Standard** | Normal priority events (Blaster). Mixed additively. |
| **2** | **Override** | High priority events (Clash). Cause forced attenuation in lower levels. |
| **3** | **System** | Critical hardware events (Power On/Off). Total control over the bus. |

---

## 5. Dynamic Profile Management (InertialProfile)
The system allows swapping the active list of actions at runtime.

1. **Profile Loading:** The `InertialProfile` injects its `InertialDefinition` into the core engines and registers its `InertialEffect` objects on the bus.
2. **Cleanup:** The bus ensures proper memory release of actions from the previous profile.
3. **Execution:** The main loop iterates over the active list, passing the `SaberDataPacket` reference to the `Test()` method of each effect.

---

## 6. Execution Logic (Abstract Pseudocode)

```cpp
// Base contract for any action in the system
class InertialEffect {
public:
    uint8_t Priority;
    virtual bool Test(const SaberDataPacket& packet) = 0; // Evaluation
    virtual void Run() = 0;                               // Rendering
};

// Main processing loop (Update Rate: ~1ms)
while (system_active) {
    SaberDataPacket currentPacket = HardwareScanner::generatePacket();
    
    for (auto& effect : activeProfile.effects) {
        if (effect->Test(currentPacket)) {
            effect->Run();
        }
    }
    
    OutputRenderer::flush();
}
```