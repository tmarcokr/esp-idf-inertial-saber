# Kinetic Gestures: Touchless Operation

**Kinetic Gestures** allow the user to control the InertialSaber OS through physical movement patterns recognized by the IMU. This reduces reliance on physical buttons and enhances the "mystical" experience.

## 1. Pattern Recognition Logic
Gestures are defined as **Sequential Actions** within the `SaberAction Bus`. They monitor a short buffer (200ms - 500ms) of kinetic data to detect specific signatures.

---

## 2. Gesture Catalog

### 2.1. Axis Twist (Ignition / Retraction)
- **Concept:** A rapid rotation of the wrist on the hilt's longitudinal axis.
- **Trigger:** 
    - `Gyroscope_Z > 500 deg/s` (Threshold).
    - Duration: `< 300ms`.
- **Action:** Triggers System Priority (Level 3) **Ignition** (if Off) or **Retraction** (if On).

### 2.2. Kinetic Thrust (Ignition)
- **Concept:** A forceful forward jab.
- **Trigger:**
    - `Accel_Y > 4.0G` (Forward acceleration).
    - Followed by `Accel_Y < -2.0G` (Sudden braking) within 100ms.
- **Action:** Triggers **Ignition**.

### 2.3. Gravity Retrieval (Profile Cycle)
- **Concept:** Pointing the saber straight up and twisting.
- **Trigger:**
    - `BladeAngle > 85°` (Pointing Up).
    - AND `Axis Twist` gesture detected.
- **Action:** Cycles to the next **InertialProfile**.

### 2.4. Force Push (Audio Modifier)
- **Concept:** A lateral shove of the hilt.
- **Trigger:**
    - `Accel_X > 3.0G` (Lateral acceleration).
- **Action:** Triggers a momentary echo/reverb increase in the **InertialSwing** engine.

---

## 3. Safety Protocols
To prevent accidental activation during intense combat:
- **Combat Lock:** Gestures can be disabled if `EnergiaTotal` has been consistently high (> 5.0G) for the last 2 seconds.
- **Confirmation window:** Some gestures may require a specific orientation (e.g., `BladeAngle`) to be valid.
