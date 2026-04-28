# SaberAction System: The Event Bus

The **SaberAction System** is the asynchronous heart of InertialSaber OS. It processes discrete events (impacts, triggers, button presses) by evaluating a stream of kinetic and interface data against a set of prioritized rules.

## 1. The Data Stream (SaberDataPacket)
At every cycle (~1.25ms), the system generates a `SaberDataPacket` containing:
- **Kinetic Data:** Instantaneous G-Force, Rotation, and Orientation.
- **Input Data:** Logical state and duration of all physical interfaces (buttons).

## 2. The InertialEffect Interface
Every discrete action in the system must implement this contract:
1. **Test(packet):** Analyzes the data stream and returns `true` if its specific trigger condition is met (e.g., "G-Force > 10.0 and duration < 20ms").
2. **Run():** Executes the associated audio/visual/haptic response.

## 3. Priority Hierarchy
To prevent chaotic outputs when multiple triggers occur, the Action Bus uses a four-level priority system:

| Priority | Category | Behavior |
| :--- | :--- | :--- |
| **0** | **Background** | Persistent sounds (Hum/Swing). Can be ducked. |
| **1** | **Standard** | Regular triggers (Auxiliary). Added to the mix. |
| **2** | **Override** | High-intensity events (Impacts). Temporarily ducks lower levels. |
| **3** | **System** | Critical hardware events (Ignition/Retraction). Total bus override. |

## 4. Asynchronous Execution
The Action Bus ensures that while the core engines (Swing/Light) provide a continuous background, the discrete effects are triggered and managed without blocking the main performance loop.
