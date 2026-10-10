# System Overview: The InertialSaber Architecture

InertialSaber OS is designed as a **Physical Simulation** rather than a state machine. This document explains how data flows from the hardware sensors to the final sensory output (light and sound).

## 1. High-Level Data Flow

The system runs an event-driven loop: the SaberAction bus wakes on every IMU sample (**100 Hz**, DMP) and on a 10 ms timeout fallback. Measured on the reference board (debug build), the bus runs at about **155 Hz** on average (128–185 Hz per 1 s window), and a bus cycle takes 70–240 µs on average (max 1.47 ms while ignited, against a 2 ms budget).

```text
[ HARDWARE ]          [ PROCESSING ]          [ RENDERING ]
  IMU Sensors  ───►  Kinetic Parser  ───►  Inertial Engines
     (Gyro/Accel)      (Metrics Calculation)      (Audio / Visual)
                             │                       ▲
                             ▼                       │
                       SaberAction Bus  ─────────────┘
                       (Discrete Events)
```

## 2. The Kinetic Foundation
The core of the system is the **Kinetic Parser**. It transforms raw IMU data into three high-level descriptors that the rest of the OS understands:

- **Kinetic Energy (`kineticEnergy`, G):** Measures the intensity of motion (linear acceleration with gravity removed).
- **Inertial Overload (`inertialOverload`, 0.0 – 1.0):** Accumulates energy over time to trigger explosive bursts.
- **Orientation Vector (`orientation`, −1.0 … +1.0):** Provides spatial context (down, horizontal, up).

See [Kinetic Metrics](KineticMetrics.md) for the definitions.

## 3. The Core Engines

### InertialSwing (Audio)
InertialSwing uses **Linear Kinetic Energy**. This allows the saber to react to thrusts, steps, and subtle movements that don't involve rotation.
- *Key Document:* [InertialSwing Specification](InertialSwing.md)

### InertialLight (Visual)
Calculates color in HSB space. It simulates a dynamic plasma blade that "heats up" (thermal bleed) and "ruptures" (burst) based on physical stress.
- *Key Document:* [InertialLight Specification](InertialLight.md)

## 4. Event Management: The Action Bus
While the engines handle continuous responses, the **SaberAction System** handles discrete events like impacts or button presses.
- Each effect has a **priority** that sets its evaluation order in a bus cycle: lower first, and equal priorities keep their registration order. It is not an arbitration level.
- It allows for **Modular Effects**, grouped in one effect set per profile.
- *Key Document:* [SaberAction Specification](SaberAction.md)

### InertialHaptics (Tactile)
Roadmap only: not implemented. See [InertialHaptics](InertialHaptics.md).

## 5. Profiles
Profiles are **data-driven**: each one is a `profile.json` file plus a sound font on the SD card. They are parsed once at boot, and a triple click switches to the next one while the blade is retracted. See [Profiles](Profiles.md).

## 6. Performance Focus
JSON is parsed only at boot, never in the bus loop. A profile switch is prepared on another core and committed by the bus with a pointer swap. The bus loop itself allocates no heap memory (only some effect triggers allocate an LED overlay) and never blocks on audio or SD I/O; the longest bus cycle measured while ignited is 1.47 ms against a 2 ms budget (see [Diagnostics](Diagnostics.md)).
