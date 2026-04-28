# SmoothSwing V2 - Technical Overview

## Introduction

SmoothSwing V2 is an advanced lightsaber swing sound algorithm that creates continuous, responsive swing sounds reacting dynamically to motion. This document provides a high-level architectural overview of the system for implementing on ESP32 platforms.

## System Architecture

```
┌─────────────────┐
│  Accelerometer  │
│   & Gyroscope   │
│   (IMU Sensor)  │
└────────┬────────┘
         │ Raw motion data (1600 Hz)
         │
         ▼
┌─────────────────┐
│ Motion Fusion   │
│  & Filtering    │
└────────┬────────┘
         │ Filtered gyro data
         │
         ▼
┌─────────────────┐
│  SmoothSwing    │
│   Algorithm     │
│  (State Machine)│
└────────┬────────┘
         │ Volume & position commands
         │
         ▼
┌─────────────────┐
│  Audio Mixing   │
│    Pipeline     │
└────────┬────────┘
         │ Mixed audio stream
         │
         ▼
┌─────────────────┐
│   DAC Output    │
│   (44.1 kHz)    │
└─────────────────┘
```

## Core Concept

SmoothSwing V2 treats swing audio as a **virtual rotating wheel** with sound files positioned at specific angular locations. As the lightsaber moves:

1. **Gyroscope measures rotation speed** (degrees/second)
2. **Virtual wheel rotates** at speed proportional to gyro
3. **Two audio players (A & B)** continuously loop swing sounds
4. **Transition zones** positioned at specific angles trigger crossfades
5. **Volume modulates** based on swing speed with power curve

This creates seamless, continuous swing sounds that feel natural and responsive.

## Key Components

### 1. Motion Processing
- **Input:** Raw accelerometer & gyroscope data from IMU
- **Processing:** Sensor fusion, filtering, swing speed calculation
- **Output:** Swing speed (degrees/second), acceleration (degrees/second²)

### 2. State Machine
Three states manage swing lifecycle:
- **OFF:** Idle, waiting for motion above threshold
- **ON:** Active swinging, crossfading between audio files
- **OUT:** Fading out, preparing for next swing

### 3. Dual Audio Players
- **Player A & B:** Always looping swing audio files
- **Crossfading:** Smooth transitions based on virtual position
- **Volume Control:** Independent volume for each player

### 4. Hum Integration
- **Ducking:** Hum volume reduces during swings
- **Smooth Blending:** Gradual transitions between hum and swing

## Audio File Requirements

### Required Files
- **Low frequency swings:** `swingl01.wav`, `swingl02.wav`, ... (looped)
- **High frequency swings:** `swingh01.wav`, `swingh02.wav`, ... (looped)
- **Hum sound:** `hum.wav` (looped continuously)

### Optional Enhancement Files
- **Accent swings:** `swng01.wav`, `swng02.wav`, ... (triggered on fast motion)
- **Accent slashes:** `slsh01.wav`, `slsh02.wav`, ... (triggered on aggressive acceleration)

**Critical Rule:** Number of low swing files must **exactly match** high swing files.

## Key Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `SwingStrengthThreshold` | 20.0 deg/s | Minimum speed to trigger swing |
| `SwingSensitivity` | 450.0 deg/s | Speed at maximum volume |
| `MaximumHumDucking` | 75.0% | Hum volume reduction during swing |
| `SwingSharpness` | 1.75 | Volume response curve exponent |
| `Transition1Degrees` | 45.0° | Width of first transition zone |
| `Transition2Degrees` | 160.0° | Width of second transition zone |
| `MaxSwingVolume` | 3.0 | Maximum swing volume multiplier |

## Algorithm Flow

```
1. Read Gyroscope Data
   ↓
2. Calculate Swing Speed: sqrt(gyro_y² + gyro_z²)
   ↓
3. Check Threshold (20 deg/s)
   ↓
4. Calculate Swing Strength: min(1.0, speed / 450)
   ↓
5. Apply Power Curve: swing_strength ^ 1.75
   ↓
6. Update Virtual Rotation: position -= speed × delta_time
   ↓
7. Check Transition Zones
   ↓
8. Crossfade Between Players A & B
   ↓
9. Apply Volumes:
   - Swing Volume = swing_strength × crossfade
   - Hum Volume = 1.0 - (swing_strength × 0.75)
   ↓
10. Mix Audio Streams
```

## Motion to Sound Relationship

### Slow Movement (< 20 deg/s)
- **Swing:** Silent (volume = 0)
- **Hum:** Full volume
- **State:** OFF

### Medium Movement (20-450 deg/s)
- **Swing:** Volume increases gradually (power curve)
- **Hum:** Volume decreases proportionally
- **State:** ON
- **Transition:** Smooth crossfade between low/high swing files

### Fast Movement (> 450 deg/s)
- **Swing:** Maximum volume (can exceed hum by 3x)
- **Hum:** Reduced to 25% (75% ducking)
- **State:** ON
- **Transition:** Rapid progression through audio files

### Very Fast/Aggressive (acceleration > 260 deg/s²)
- **Accent Effects:** Slash sounds triggered
- **Swing:** Continues as normal
- **Hum:** Remains ducked

## Technical Requirements for ESP32 Implementation

### Hardware Requirements
- **IMU Sensor:** I2C accelerometer + gyroscope (e.g., MPU6050, LSM6DS3)
- **DAC:** I2S or I2C DAC for audio output
- **Storage:** SD card or SPI flash for sound files
- **Sample Rate:** 44.1 kHz audio output

### Timing Requirements
- **Gyro Sampling:** 800-1600 Hz
- **Audio Callback:** ~1 ms (44 samples @ 44.1 kHz)
- **Motion Processing:** < 100 μs per cycle

### Memory Requirements
- **Audio Buffers:** 2 × 512 bytes per player (4 players minimum)
- **DMA Buffer:** 176 bytes (88 samples × 2 bytes)
- **File Buffers:** 512 bytes per WAV decoder

### Processing Requirements
- **Square Root:** Fast sqrt implementation needed (2-4 per frame)
- **Trigonometry:** None required (uses angular tracking)
- **Floating Point:** Moderate usage (volume calculations)

## Implementation Phases

### Phase 1: Motion Foundation
1. Initialize IMU sensor (I2C)
2. Implement gyro data acquisition at 800+ Hz
3. Calculate swing speed: `sqrt(y² + z²)`
4. Test threshold detection

### Phase 2: Audio Foundation
1. Initialize I2S/I2C DAC
2. Implement 44.1 kHz audio callback
3. Create WAV file loader
4. Test looping playback

### Phase 3: Basic Mixing
1. Implement dual audio players
2. Add volume control per player
3. Mix two streams to single output
4. Test crossfading

### Phase 4: SmoothSwing Algorithm
1. Implement virtual rotation tracking
2. Add transition zone detection
3. Link motion data to volume control
4. Implement power curve (swing sharpness)

### Phase 5: Hum Integration
1. Add dedicated hum player
2. Implement hum ducking algorithm
3. Test smooth blending

### Phase 6: Optimization & Polish
1. Add accent swing/slash detection
2. Optimize CPU usage
3. Add configuration system
4. Fine-tune parameters

## Key Design Insights

### Virtual Rotation Model
The algorithm doesn't play swing sounds on-demand. Instead, it maintains two continuously looping players at zero volume, with volume fading in/out based on motion. This eliminates timing issues and creates seamless transitions.

### Separation Control
Low-to-high and high-to-low transitions can have different angular separations (default: 180° each). This allows asymmetric response to blade direction changes.

### Random Variation
- Random pair selection prevents repetitive patterns
- Random transition offset (10-60°) adds variation
- Random A/B swap provides additional variety

### Smooth Fading
All volume changes use exponential curves with configurable fade times (default: 0.2s). This prevents audio clicks and creates natural sound evolution.

## Next Steps

For detailed implementation:
- **Motion Processing:** See `motion-processing.md`
- **Audio System:** See `audio-system.md`
- **SmoothSwing Algorithm:** See `smoothswing-algorithm.md`
- **Audio Mixing:** See `audio-mixing-pipeline.md`
- **Sound Font Structure:** See `sound-font-structure.md`

## References

- **Original Implementation:** ProffieOS `sound/smooth_swing_v2.h`
- **Configuration:** ProffieOS `sound/smooth_swing_config.h`
- **Thexter's SmoothSwing:** Original algorithm designer
