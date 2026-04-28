# InertialSwing Engine: Physics-Based Audio

The **InertialSwing Engine** is the audio core of InertialSaber OS. It moves away from static swing sounds to a dynamic mixing model driven by **Linear Kinetic Energy**.

## 1. Mixing Philosophy
Instead of playing "swing tracks," the engine balances two primary textures based on intensity:
- **SwingL (Low):** Deep, bass-heavy sounds representing mass and power.
- **SwingH (High):** Sharp, high-frequency sounds representing speed and energy.

## 2. The Power Curve (Inertial Crossfade)
The mixer calculates the volume and balance for every frame (800Hz) using the following thresholds:

| Energy (G) | Audio Behavior |
| :--- | :--- |
| **< 0.5 G** | Silence (Only Hum is audible). |
| **0.5 - 2.0 G** | `SwingL` dominates. Volume scales up linearly. |
| **2.0 - 4.0 G** | Crossfade: `SwingL` fades out, `SwingH` fades in. |
| **> 4.0 G** | `SwingH` at maximum volume. |

### Balance Formula:
`MixFinal = clamp((EnergiaTotal - 2.0) / (4.0 - 2.0), 0.0, 1.0)`

## 3. Gravity Tonal Modulation
The `BladeAngle` acts as a spectral shifter, adjusting the character of the sound based on orientation.
- **Upward (+90°):** Shifts +20% weight towards `SwingH` (Ethereal/Lighter).
- **Downward (-90°):** Shifts +20% weight towards `SwingL` (Heavy/Roaring).

## 4. Inertial Overload (Bursts)
Sustained physical effort "charges" the system. When the `TanqueOverload` reaches 1.0 (100%), the engine triggers a high-priority **Inertial Burst**.
- **Trigger:** `TanqueOverload == 1.0`.
- **Action:** Plays an aggressive kinetic sound at maximum volume.
- **Reset:** The tank resets to zero and enters a short cooldown to prevent repetition.

## 5. Zero-Volume Swapper
To maintain performance and avoid I2S bus clicks, the engine only swaps audio files (e.g., when changing profiles or banks) when the calculated volume is exactly zero.
