# InertialProfiles: Identity & Configuration

InertialSaber OS uses a high-performance configuration model where each saber identity (color, sound, behavior) is a **Compiled C++ Class**. This approach ensures near-zero latency and deterministic behavior on the ESP32.

## 1. Profile Structure
A Profile consists of two main parts:
1. **InertialDefinition:** Static configuration parameters for the engines (HSB colors, G-Force limits, Audio paths).
2. **Action Collection:** A list of `InertialEffect` objects (impacts, triggers) that the profile supports.

## 2. Resource Management
While the logic is compiled, large resources (WAV files) are referenced via paths on the SD card.

### Standard Directory Structure:
```text
/SD/profiles/[profile_name]/
├── motion/      (hum.wav, swingh/, swingl/)
├── impact/      (Kinetic collision effects)
├── thrust/      (High-speed linear effects)
└── auxiliary/   (Optional triggers)
```

## 3. The Lifecycle of a Profile
- **Instantiation:** Profiles are pre-allocated in memory or instantiated on demand.
- **Loading:** When a profile is activated, it inyects its `InertialDefinition` into the audio/visual engines and registers its effects into the **Action Bus**.
- **Execution:** Once loaded, the engines and the bus use the profile's specific parameters to calculate the real-time response.

## 4. Example: The "Sith Lord" Profile
- **Definition:** Deep Red (Hue 0), Low Overload Sensitivity (requires more force), Slow Breathing.
- **Effects:** High-priority "Override" Impacts with complementary (Cyan) flashes.
- **Audio:** Aggressive, low-frequency hum textures.
