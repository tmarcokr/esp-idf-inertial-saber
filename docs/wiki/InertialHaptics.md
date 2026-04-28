# InertialHaptics Engine: Kinetic Feedback System

> **🚧 Future Roadmap:** This engine is currently in the conceptual design phase. It outlines our ambition to add high-fidelity tactile feedback to the InertialSaber OS in future releases.

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
**Logic:** Modulated by `TanqueOverload`.
- **Behavior:** As the inertia tank fills, the vibration frequency increases from a deep, calm thrum (30Hz) to a high-pitched, nervous electricity-like buzz (150Hz).
- **Goal:** To make the user feel the "stress" of the energy containment before a burst.

### State B: Kinetic Friction (Movement)
**Logic:** Modulated by `EnergiaTotal`.
- **Behavior:** The intensity (amplitude) of the vibration is directly proportional to the G-Force applied.
- **Goal:** To simulate the feeling of moving a heavy, energized object through the air.

### State C: Kinetic Kick (Recoil/Burst)
**Logic:** Discrete trigger when `TanqueOverload == 1.0`.
- **Behavior:** A single, high-amplitude low-frequency square wave pulse (Recoil).
- **Goal:** To simulate the physical "kickback" of a plasma rupture.

## 4. Technical Specifications

### Input Metrics:
- **Frequency (Hz):** `BaseFrequency + (TanqueOverload * 120)`
- **Amplitude (Gain):** `(EnergiaTotal / MaxG) * HapticGain`

### Priority System Integration:
Haptic events follow the same **Priority System (0-3)** as the Action Bus, ensuring that a "Kinetic Kick" (Priority 2) overrides the "Plasma Resonance" (Priority 0).

## 5. Synergy with Other Engines
The InertialHaptics Engine works in perfect phase with the **InertialSwing Engine**. When a sound is generated, the haptic engine produces the corresponding "mechanical weight" of that sound, creating a unified sensory experience.
