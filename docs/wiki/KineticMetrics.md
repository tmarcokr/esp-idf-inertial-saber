# Kinetic Metrics: The Language of Motion

InertialSaber OS translates raw sensor data into a unified "Kinetic Language." These metrics are the primary inputs for all sound and light calculations.

## 1. EnergiaTotal (G-Force)
This is the magnitude of the linear acceleration vector acting on the hilt. It represents the intensity of the movement.

- **Unit:** G (standard gravity).
- **Calculation:** `sqrt(accel_x^2 + accel_y^2 + accel_z^2) - 1.0` (Gravity subtracted).
- **Range:** 0.0 (Rest) to 16.0+ (Intense swing).
- **Usage:** Controls volume (InertialSwing) and flicker intensity (InertialLight).

## 2. TanqueOverload (Integrated Inertia)
A virtual accumulator that represents the "stress" placed on the plasma containment. It requires sustained, high-energy movement to fill.

- **Range:** 0.0 to 1.0 (Full).
- **Logic:**
    - **Charging:** Fills when `EnergiaTotal > Threshold` (e.g., 3.5G).
    - **Draining:** Empties quickly when energy drops below the threshold.
- **Usage:** Triggers **Inertial Bursts** (Sound) and **Plasma Ruptures** (Light) at 1.0.

## 3. BladeAngle (Spatial Orientation)
The tilt of the blade relative to the horizon, derived from gravity vector analysis.

- **Unit:** Degrees (-90° to +90°).
    - **+90°:** Tip pointing straight up.
    - **0°:** Blade horizontal.
    - **-90°:** Tip pointing straight down.
- **Usage:** Tonal modulation of audio and breathing frequency of the light engine.

## 4. Derived Vectors (Rotation Speed)
While the system prioritizes linear energy, the angular velocity (deg/s) from the Gyroscope is used for fine-tuning stability and secondary triggers.

- **Usage:** Differentiating between a "spin" and a "thrust."
