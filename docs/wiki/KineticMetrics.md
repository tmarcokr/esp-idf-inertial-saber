# Kinetic Metrics: The Language of Motion

InertialSaber OS translates raw sensor data into a unified "Kinetic Language." These metrics are the primary inputs for all sound and light calculations. They are computed by the IMU adapter and the bus, and every effect reads them from the bus packet (`SaberDataPacket`, see [SaberAction](SaberAction.md) §2.1).

## 1. Kinetic Energy (`kineticEnergy`)
This is the magnitude of the linear acceleration vector acting on the hilt, with gravity removed. It represents the intensity of the movement.

- **Unit:** G (standard gravity).
- **Calculation:** `sqrt(lin_x² + lin_y² + lin_z²)`, where `lin` is the linear acceleration computed by the MPU-6050 DMP (accelerometer minus the DMP gravity estimate). During start-up a fallback is used instead (§5).
- **Deadband:** values below `sensor.kinetic_deadband_g` (default 0.25 G) are reported as 0.
- **Range:** 0.0 (rest) upwards. The accelerometer runs at ±2 g per axis, which bounds the reachable maximum.
- **Usage:** Controls swing volume and tonal balance (InertialSwing), flicker intensity (InertialLight), the Inertial Overload charge and the clash detection.
*(For implementation examples, see the [InertialSwing](InertialSwing.md) and [InertialLight](InertialLight.md) engines).*

## 2. Inertial Overload (`inertialOverload`)
A virtual accumulator that represents the "stress" placed on the plasma containment. It requires sustained, high-energy movement to fill.

- **Range:** 0.0 to 1.0 (Full).
- **Logic:**
    - **Charging:** Fills when `kineticEnergy > overload.threshold_g` (default 1.0 G).
    - **Draining:** Empties when the energy is at or below the threshold.
- **Usage:** At 1.0 it fires an **Inertial Burst** (`inertialBurst`, true for one bus cycle): a burst sound (InertialSwing) and a **Plasma Rupture** flash (InertialLight).
*(For the detailed physics integration logic, see the [Inertial Overload Specification](InertialOverload.md)).*

## 3. Orientation Vector (`orientation`)
The tilt of the blade relative to the horizon, derived from the DMP attitude.

- **Source:** the Euler **roll** angle of the DMP quaternion, in degrees, clamped to −90° … +90° by the bus.
- **Unit:** none, normalised: `orientation = roll / 90°`, so −1.0 … +1.0.
    - **+1.0:** Tip pointing straight up.
    - **0.0:** Blade horizontal.
    - **−1.0:** Tip pointing straight down.
- **Usage:** Tonal modulation of audio and breathing frequency of the light engine. Both convert it back to an angle with `orientation × π/2` before taking its sine.
*(For gravity modulation formulas, see [InertialSwing](InertialSwing.md) and [InertialLight](InertialLight.md)).*

## 4. Derived Vectors (Rotation Speed, `axisRotation`)
While the system prioritizes linear energy, the angular velocity from the gyroscope is distributed for fine-tuning stability and secondary triggers.

- **Unit:** °/s per axis (X, Y, Z). Axes below `sensor.rotation_deadband_dps` (default 15 °/s) are reported as 0.
- **Usage:** Differentiating between a "spin" and a "thrust" (not used by the current effects).

## 5. Start-Up Behaviour
After power-up the DMP needs time before its gravity estimate is reliable. Until then its linear acceleration contains part of the gravity, which would look like motion. The IMU adapter therefore publishes **fallback values** first:

| Metric | Fallback value (until settled) | Value after settling |
| :--- | :--- | :--- |
| `kineticEnergy` | `\| \|a_raw\| − 1 g \|` (raw accelerometer magnitude minus 1 g) | DMP linear acceleration magnitude (§1) |
| `orientation` | Roll from the raw accelerometer tilt | DMP roll (§3) |

- **Settling:** the adapter switches to the DMP values once, with no blending, at the first quasi-static sample after the DMP gravity vector has stayed within **4°** of the measured acceleration for **20 consecutive quasi-static samples** (200 ms at 100 Hz). A sample is quasi-static when `| |a_raw| − 1 g |` < 0.08 g and every gyroscope axis is below 20 °/s.
- **Ceiling:** 20 s after the first IMU sample the switch is forced at the next quasi-static sample. If none arrives by 40 s, the switch is forced at the next sample.
- **Effect on the user:** the saber can be ignited and swung at once. The fallback energy under-reads lateral motion, so swings are weaker until the DMP settles. Settling needs the saber to be held still briefly; while it keeps moving, the 20 s ceiling applies.
- **Bus warm-up:** independently, the bus forces the motion values to rest for the first 300 ms after power-up ([SaberAction](SaberAction.md) §2.1).
- **Metrics:** `boot,imu_settle_ms` (time of the switch), `count,imu_fallback_samples` and `time,ke_quasi_static_settled` ([Diagnostics](Diagnostics.md)). Typical `imu_settle_ms` values are pending board validation.
