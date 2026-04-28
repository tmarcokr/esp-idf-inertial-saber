# Functional Specification: Profile System (InertialProfile)

## 1. System Overview
The `InertialProfile` is the component in charge of defining the aesthetic, sonic, and reactive identity of the saber. Unlike systems based on external configuration files, this model uses **Compiled Classes** to guarantee maximum execution speed on the ESP32.

Each profile encapsulates the logic of the inertial engines (`InertialDefinition`) and the collection of discrete effects (`InertialEffect`) that the saber can execute, acting as the "Personality Container" of the system.

---

## 2. Resource Organization on the SD
Although the intelligence and trigger rules are compiled, heavy resources (`.wav` audio files) are stored on the SD card following a hierarchical structure that the profile references dynamically.

```text
/SD/
└── profiles/
    └── sith_lord/
        ├── impact/      <-- Files for clash InertialEffect
        ├── blaster/     <-- Files for blast InertialEffect
        ├── force/       <-- Files for Force effects
        └── motion/      <-- Resources for the core Inertial Engines
            ├── hum.wav
            ├── swingh/  <-- High variations (swingH)
            └── swingl/  <-- Low variations (swingL)
```

---

## 3. Class Architecture: The Effect Contract
The system relies on an effects interface that allows the `SaberAction System` to evaluate and execute actions in a modular and asynchronous manner.

### `InertialEffect` Interface
Each effect (clash, blast, etc.) is a class that implements two fundamental methods:

*   **Test(SaberDataPacket):** Evaluation method that receives the sensor stream at high frequency (~800Hz) and returns a boolean if conditions (G-Force, rotation, or button input) are met to trigger the effect.
*   **Run():** Execution method that coordinates the audio output (WAV) and the associated light response (LED) at the moment of the trigger.

```cpp
// Logical structure of the effect contract
class InertialEffect {
public:
    uint8_t Priority;         // Priority level (0-3: Background to System)
    virtual bool Test(const SaberDataPacket& packet) = 0; // When does it occur?
    virtual void Run() = 0;   // What does it do? (Audio + Light)
};
```

---

## 4. Inertial Engine Definition (InertialDefinition)
This class acts as the technical configuration blueprint for the core `InertialSwing` and `InertialLight` engines. It defines the physical parameters and resource paths that the algorithms will use for that specific profile.

### `InertialDefinition` Properties
*   **Inertial Parameters:** G-Force limits, accumulator sensitivity (`OverloadSensitivity`), and drain rates (`OverloadDrainRate`).
*   **Light Parameters:** Base color (HSB), "breathing" frequency (`IdleBaseFreq`), and pulse depth (`IdlePulseDepth`).
*   **Core Audio Mapping:** Compiled lists with paths to the directories or files of `hum`, `swingH`, and `swingL` on the SD card.

---

## 5. Load Management and the Action Bus
Profiles are not scanned at runtime; they are instantiated and loaded via the `.load()` method, which injects the logic and resources into the `SaberAction Bus`.

### `InertialProfile.load()` Logic
When the user selects a profile (e.g., `SithLord.load()`), the following injection actions are executed:

1.  **Core Configuration:** Delivers the `InertialDefinition` to the audio and light engines to establish the physical limits and begin loading the base sounds (Hum/Swings).
2.  **Action Registration:** The profile delivers its collection of `InertialEffect` objects to the `SaberAction Bus`.
3.  **Active Evaluation:** The Bus begins immediately passing the `SaberDataPacket` through the `Test()` method of each registered effect to detect triggers.

---

## 6. Functional Layer Summary

| Layer | Component | Nature | Main Function |
| :--- | :--- | :--- | :--- |
| **Resources** | Audio WAV | SD Card | Binary sound data organized by folders. |
| **Configuration** | `InertialDefinition` | Compiled Class | Defines physical limits, HSB colors, and core audio paths. |
| **Logic** | `InertialEffect` | Compiled Class | Contains the Test (condition) and Run (rendering). |
| **Identity** | `InertialProfile` | Compiled Class | Groups the inertial definition and effect list. |
| **Control** | `SaberAction Bus` | Core Engine | Evaluates data stream against profile rules. |
