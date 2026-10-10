# InertialHaptics Engine: Kinetic Feedback System

> **🚧 Future Roadmap:** This engine is **not implemented**: there is no haptic hardware driver, component or bus integration. This page is a conceptual design for a future release.

The **InertialHaptics Engine** is the third sensory pillar of InertialSaber OS. It translates the internal tension of the plasma and the kinetic energy of movement into physical vibrations felt in the user's hands.

## 1. Functional Philosophy
InertialHaptics is not a simple notification system. It is a **Physical Simulation** designed to give the hilt "perceived mass" and "mechanical life." It uses real-time synthesis to ensure zero-latency synchronization between the saber's sound and its feel.

## 2. Hardware Architecture (Recommended)
To achieve high-fidelity "Inertial" feedback, the system is designed for a **Dual-Output Architecture**:

- **Primary I2S Channel:** Dedicated to the **InertialSwing Engine** (Audio/Speaker).
- **Secondary I2S Channel:** Dedicated to the **InertialHaptics Engine** (Kinetic Transducer/Exciter).

Using a dedicated **Kinetic Transducer (Exciter)** instead of a standard vibration motor allows the system to reproduce specific frequencies (20Hz - 200Hz) that match the plasma's resonance.

## 3. Kinetic Feedback States

### State A: Plasma Resonance (Idle/Overload Stress)
**Logic:** Modulated by `inertialOverload` (0.0 – 1.0).
- **Behavior:** As the inertia tank fills, the vibration frequency increases from a deep, calm thrum (30Hz) to a high-pitched, nervous electricity-like buzz (150Hz).
- **Goal:** To make the user feel the "stress" of the energy containment before a burst.

### State B: Kinetic Friction (Movement)
**Logic:** Modulated by `kineticEnergy` (G).
- **Behavior:** The intensity (amplitude) of the vibration is directly proportional to the G-Force applied.
- **Goal:** To simulate the feeling of moving a heavy, energized object through the air.

### State C: Kinetic Kick (Recoil/Burst)
**Logic:** Discrete trigger on `inertialBurst` (the Inertial Overload reached 1.0).
- **Behavior:** A single, high-amplitude low-frequency square wave pulse (Recoil).
- **Goal:** To simulate the physical "kickback" of a plasma rupture.

## 4. Technical Specifications

### Input Metrics:
- **Frequency (Hz):** `BaseFrequency + (inertialOverload * 120)`
- **Amplitude (Gain):** `(kineticEnergy / MaxG) * HapticGain`

### Priority System Integration:
On the Action Bus a priority only sets the **evaluation order** (lower first; equal priorities keep their registration order), not an arbitration level (see [SaberAction](SaberAction.md) §4). A haptic engine would have to arbitrate itself, so that a "Kinetic Kick" overrides the "Plasma Resonance".

## 5. Synergy with Other Engines
The InertialHaptics Engine works in perfect phase with the **InertialSwing Engine**. When a sound is generated, the haptic engine produces the corresponding "mechanical weight" of that sound, creating a unified sensory experience.
