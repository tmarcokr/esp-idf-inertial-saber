# esp-idf-inertial-saber (InertialSaber OS)

**InertialSaber OS** is a high-performance, open-source operating system for lightsabers, specifically engineered for the ESP32 family (S3/C6). InertialSaber OS utilizes a **Native Physics Engine** to deliver an organic, high-fidelity experience driven by real-time kinetic data.

## 🚀 The Core Philosophy: "Inertial Logic"

The system is built on the principle that a lightsaber should feel like a contained column of plasma reacting to physical laws. Every sound, flash, and vibration is a direct mathematical result of:
- **Linear Kinetic Energy (G-Force)**
- **Inertial Overload & Burst (Kinetic Accumulation)**
- **Gravitational Orientation (Spatial Awareness)**

## 🛠️ System Architecture

InertialSaber OS is modular and extensible, organized into specialized engines and management systems:

### 1. [InertialSwing Engine](./docs/wiki/InertialSwing.md)
The audio core. It replaces standard "swing sounds" with a physics-based mixer that calculates volume and tonal balance based on the kinetic energy applied to the hilt.

### 2. [InertialLight Engine](./docs/wiki/InertialLight.md)
The visual core. Working in HSB (Hue, Saturation, Brightness) space, it simulates plasma behavior. From the "Live Breathing" pulse at rest to the "Thermal Bleed" (whitening) during high-speed movement and "Plasma Rupture" flashes.

### 3. [Inertial Overload Accumulator](./docs/wiki/InertialOverload.md)
The "Inertia Tank". A physics-driven accumulator that tracks sustained movement intensity. Once fully charged, it triggers high-priority **Inertial Bursts**, creating a bridge between continuous motion and explosive climax events.

### 4. [SaberAction System (The Action Bus)](./docs/wiki/SaberAction.md)
The asynchronous event dispatcher. It processes sensor data at high frequency (~800Hz) and evaluates active `InertialEffects` (impacts, thrusts, etc.) based on prioritized physical triggers.

### 5. [InertialHaptics Engine](./docs/wiki/InertialHaptics.md)
The tactile core. It uses real-time low-frequency synthesis to simulate plasma mass, friction, and recoil.

### 6. [InertialProfile System](./docs/wiki/Profiles.md)
A high-performance configuration model. Profiles are compiled C++ classes that encapsulate specific physics definitions and effect collections.

## 📂 Project Structure

- `main/`: Core application logic and ISample implementations.
- `components/`: Hardware wrappers and peripheral drivers.
- `docs/wiki/`: Official functional and technical specifications (The Source of Truth).
- `.agents/`: Expert AI configurations for development, hardware, and quality assurance.

## 📑 Documentation Index (Wiki)

Explore the technical depth of InertialSaber OS:
1. [**System Overview**](./docs/wiki/Home.md) - The "Big Picture" and data flow.
2. [**Kinetic Metrics**](./docs/wiki/KineticMetrics.md) - Understanding Energy, Overload, and Angle.
3. [**Inertial Overload**](./docs/wiki/InertialOverload.md) - Detailed physics of the accumulation tank.
4. [**Audio Specification**](./docs/wiki/InertialSwing.md) - Deep dive into the InertialSwing Engine.
5. [**Visual Specification**](./docs/wiki/InertialLight.md) - Deep dive into the InertialLight Engine.
6. [**Haptic Specification**](./docs/wiki/InertialHaptics.md) - Deep dive into the InertialHaptics Engine.
7. [**Kinetic Effects**](./docs/wiki/KineticEffects.md) - Discrete events and HSB modifiers.
8. [**Kinetic Gestures**](./docs/wiki/KineticGestures.md) - Touchless operation and IMU patterns.
9. [**Profiles & Configuration**](./docs/wiki/Profiles.md) - How to define saber identities.
10. [**Action Bus & Effects**](./docs/wiki/SaberAction.md) - Event processing and trigger logic.


---

**Developed for enthusiasts, engineered for performance.**
*InertialSaber OS: Feel the physics.*
