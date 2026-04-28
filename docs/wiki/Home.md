# System Overview: The InertialSaber Architecture

InertialSaber OS is designed as a **Physical Simulation** rather than a state machine. This document explains how data flows from the hardware sensors to the final sensory output (light and sound).

## 1. High-Level Data Flow

The system operates in a continuous loop at approximately **800Hz** to ensure zero-latency response.

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

- **EnergiaTotal (G-Force):** Measures the intensity of motion.
- **TanqueOverload (Inertia):** Accumulates energy over time to trigger explosive bursts.
- **BladeAngle (Gravity):** Provides spatial context (up, down, horizontal).

## 3. The Core Engines

### InertialSwing (Audio)
InertialSwing uses **Linear Kinetic Energy**. This allows the saber to react to thrusts, steps, and subtle movements that don't involve rotation.
- *Key Document:* [InertialSwing Specification](InertialSwing)
- *Interactive Demo:* [🎮 Play Swing Simulation](../system_definition/InertialSwing_Simulation.html)

### InertialLight (Visual)
Calculates color in HSB space. It simulates a dynamic plasma blade that "heats up" (thermal bleed) and "ruptures" (burst) based on physical stress.
- *Key Document:* [InertialLight Specification](InertialLight)
- *Interactive Demo:* [🎮 Play Light Simulation](../system_definition/InertialLight_Simulation.html)

## 4. Event Management: The Action Bus
While the engines handle continuous responses, the **SaberAction System** handles discrete events like impacts or button presses.
- It uses a **Priority System (0-3)** to manage resource contention.
- It allows for **Modular Effects** that can be added or removed per profile.

## 5. Performance Focus
By using **Compiled Profiles** and an **Action Bus** architecture, InertialSaber OS avoids the overhead of complex file parsing during operation, making it ideal for the limited but efficient resources of the ESP32.

<!-- Trigger wiki sync 2: testing permissions -->
