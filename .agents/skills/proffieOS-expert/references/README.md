# SmoothSwing V2 Technical Documentation

## Overview

This documentation provides a complete technical blueprint for implementing SmoothSwing V2 lightsaber sound effects on ESP32 platforms (ESP32-C6/S3). The information is reverse-engineered from ProffieOS to enable independent implementation with I2C DAC and accelerometer.

## Documentation Structure

### 1. [smoothswing-overview.md](smoothswing-overview.md)
**Start here** for high-level understanding.

**Contents:**
- System architecture diagram
- Core concepts (virtual rotation model)
- Key components overview
- Audio file requirements
- Implementation phases
- Technical requirements summary

**Audience:** Everyone - provides the big picture before diving into details.

---

### 2. [motion-processing.md](motion-processing.md)
**Motion sensor integration and event detection.**

**Contents:**
- IMU sensor specifications (LSM6DS3H, MPU6050, etc.)
- Sample rates and data formats (1600 Hz recommended)
- Axis orientation and coordinate systems
- Filtering algorithms (BoxFilter, Extrapolator, Complementary Filter)
- Swing detection algorithm (250 deg/s threshold)
- Clash detection (dynamic threshold with gyro compensation)
- Other motion effects (twist, shake, blade angle, spin)
- Complete pseudocode implementations

**Key Sections:**
- **Swing Speed Calculation:** `sqrt(gyro_y² + gyro_z²)`
- **Clash Detection:** Combined accelerometer + gyroscope approach
- **Sensor Fusion:** Complementary filter for gravity tracking

**Read this if:** You need to integrate IMU sensors and process motion data.

---

### 3. [audio-system.md](audio-system.md)
**Audio hardware and DAC implementation.**

**Contents:**
- Audio specifications (44.1 kHz, 16-bit, mono)
- Hardware options (I2S DAC recommended for ESP32)
- DMA and interrupt-driven audio
- Buffer architecture (multi-level buffering)
- WAV file format support
- Audio stream architecture
- Complete audio pipeline flow
- ESP32-specific implementation guidance

**Key Sections:**
- **I2S Configuration:** ESP32 setup for MAX98357A, PCM5102A
- **Buffer Sizes:** 44 samples per DMA buffer, 512 bytes per player
- **ISR Timing:** 1 ms interrupt rate, <400 μs processing time

**Read this if:** You need to implement audio output with DAC.

---

### 4. [smoothswing-algorithm.md](smoothswing-algorithm.md)
**Core SmoothSwing V2 algorithm - the heart of the system.**

**Contents:**
- Virtual rotation model explained
- Configuration parameters (all 11 tunable values)
- Complete algorithm flow with pseudocode
- State machine (OFF → ON → OUT)
- Mathematical formulas
- Volume calculations
- Crossfading algorithm
- Visual diagrams
- Tuning guidelines
- Complete C++ implementation example

**Key Sections:**
- **Swing Strength:** `min(1.0, speed / SwingSensitivity)`
- **Power Curve:** `mixhum = swing_strength ^ SwingSharpness`
- **Crossfade:** Position-based blending between A/B players
- **Virtual Rotation:** `position -= speed × delta_time`

**Read this if:** You're implementing the actual SmoothSwing algorithm.

---

### 5. [audio-mixing-pipeline.md](audio-mixing-pipeline.md)
**How multiple sounds mix together.**

**Contents:**
- AudioDynamicMixer implementation
- Stream combination (summing + compression)
- Volume control system (14-bit precision)
- Clipping prevention (3-stage protection)
- Motion integration (hum ducking, swing emphasis)
- Polyphonic vs monophonic handling
- Effect prioritization
- Complete pipeline pseudocode

**Key Sections:**
- **Dynamic Compression:** `output = input × volume / (√avg + 100)`
- **Hum Ducking:** `hum_volume = 1.0 - mixhum × 0.75`
- **Smooth Fading:** Prevents audio clicks during transitions

**Read this if:** You need to mix multiple audio streams and prevent clipping.

---

### 6. [sound-font-structure.md](sound-font-structure.md)
**Sound font organization and requirements.**

**Contents:**
- SD card directory structure
- File naming patterns (3 organizational styles)
- SmoothSwing V2 specific files (swingl/swingh pairs)
- Hum sound requirements
- Complete effect list
- WAV file specifications
- Font type detection (NEC vs Plecter)
- Configuration files (smoothsw.ini)
- Sound design guidelines
- Common issues and solutions

**Key Sections:**
- **Required Files:** Minimum font structure
- **Naming Rules:** Sequential numbering, no gaps
- **Audio Format:** 44.1 kHz, 16-bit, mono WAV recommended
- **Loop Points:** Critical for seamless SmoothSwing

**Read this if:** You're preparing sound fonts for your lightsaber.

---

### 7. [effects-catalog.md](effects-catalog.md)
**Complete catalog of all ProffieOS motion and sound effects.**

**Contents:**
- Motion Detection System (Fusor class capabilities)
- 9 Motion-Based Effects with algorithms:
  - Clash Detection (dynamic threshold, audio suppression)
  - Stab Detection (2× forward vs lateral, <150°/s)
  - Twist Detection & Gestures (200°/s X-axis, patterns)
  - Shake Detection (5 strokes, <250ms separation)
  - Swing Detection (250°/s start, 100°/s stop)
  - Spin Detection (360°/s threshold)
  - Blade Angle Effects (-π/2 to π/2 range)
  - Force Push/Thrust (linear acceleration)
  - Battle Mode 2.0 (intelligent clash/lockup)
- Lockup System (5 types: Normal, Drag, Melt, Lightning Block, Armed)
- Complete configuration reference (all thresholds and parameters)
- Implementation complexity ratings (⭐ to ⭐⭐⭐⭐⭐)

**Key Sections:**
- **Clash Formula:** Dynamic threshold with gyro/audio compensation
- **Gesture Recognition:** Stroke patterns and timing windows
- **Real Code References:** Every algorithm cited with file:line

**Read this if:** You want to implement effects beyond SmoothSwing, or need algorithm specifications.

---

### 8. [sound-effects-reference.md](sound-effects-reference.md)
**Complete reference for all 63 ProffieOS sound effects.**

**Contents:**
- Organized by 9 categories:
  - Core Sounds (boot, hum, font)
  - Ignition/Retraction (preon, poweron, out, in, postoff)
  - Impact Sounds (clash, stab, blast)
  - Sustained Effects (lockup, drag, melt, lb with begin/loop/end)
  - Motion Sounds (swing, spin, smoothswing)
  - Special Effects (force, quote, blade detect)
  - UI/System (color change, battery, menu)
  - Thermal Detonator & Blaster modes
- File naming conventions and patterns
- Polyphonic vs Monophonic variants
- Font type detection (NEC vs Plecter)
- Effect fallback chains
- Quick reference table with all effects

**Key Sections:**
- **Complete Effect List:** All 63 effects with file names
- **Font Detection Logic:** How ProffieOS identifies font types
- **Naming Patterns:** FLAT, SUBDIRS, NONREDUNDANT_SUBDIRS
- **Code References:** Every effect cited with effect.h line numbers

**Read this if:** You're creating sound fonts, or need to know which sound files are required for effects.

---

### 9. [implementation-roadmap.md](implementation-roadmap.md)
**Practical 6-phase implementation guide for ESP32.**

**Contents:**
- Phase 1: Foundation (✓ Complete - hum playback, basic setup)
- Phase 2: Basic Motion Effects (1-2 weeks - clash, swing)
- Phase 3: SmoothSwing V2 (1 week - virtual disc, crossfading)
- Phase 4: Advanced Motion (2-3 weeks - stab, twist, shake, blade angle)
- Phase 5: Lockup System (1-2 weeks - begin/loop/end sequences)
- Phase 6: Polish & Advanced (1-2 weeks - multi-blast, force, battle mode)
- Resource requirements (RAM/CPU per phase)
- Effect dependencies diagram
- Testing procedures per phase
- Troubleshooting guide
- Performance optimization for ESP32

**Key Sections:**
- **Step-by-Step Code:** Ready-to-implement examples for each effect
- **Progress Tracking:** Checkboxes for each deliverable
- **ESP32-Specific:** DMA setup, RTOS tasks, memory allocation
- **Realistic Timelines:** 6-10 weeks total (part-time)

**Read this if:** You're implementing effects on ESP32 and want a clear path from beginner to complete lightsaber.

---

## Implementation Roadmap

### Phase 1: Foundation (Week 1-2)
1. **Hardware Setup**
   - Connect IMU sensor (I2C)
   - Connect I2S DAC
   - Prepare SD card with test fonts
   - Documents: `audio-system.md`, `motion-processing.md`

2. **Basic Testing**
   - Read gyroscope data at 800+ Hz
   - Output test tone at 44.1 kHz
   - Verify DMA audio works
   - Test swing speed calculation

### Phase 2: Audio Foundation (Week 3-4)
1. **WAV Playback**
   - Implement WAV file decoder
   - Create audio buffer system
   - Test looping playback
   - Documents: `audio-system.md`, `sound-font-structure.md`

2. **Basic Mixing**
   - Implement dual player system
   - Add volume control with fading
   - Test crossfading
   - Document: `audio-mixing-pipeline.md`

### Phase 3: SmoothSwing Core (Week 5-6)
1. **Algorithm Implementation**
   - Implement virtual rotation tracking
   - Add transition zone detection
   - Link motion to volume control
   - Apply power curves
   - Document: `smoothswing-algorithm.md`

2. **Motion Integration**
   - Implement sensor fusion
   - Add filtering (BoxFilter, Extrapolator)
   - Test swing detection
   - Document: `motion-processing.md`

### Phase 4: Full Integration (Week 7-8)
1. **Complete Mixing**
   - Add hum player
   - Implement hum ducking
   - Add dynamic compression
   - Test polyphonic mixing
   - Document: `audio-mixing-pipeline.md`

2. **Additional Effects**
   - Clash detection
   - Accent swings/slashes
   - Other motion effects
   - Document: `motion-processing.md`

### Phase 5: Optimization & Polish (Week 9-10)
1. **Performance**
   - Profile CPU usage
   - Optimize critical paths
   - Tune buffer sizes
   - All documents

2. **Configuration**
   - Add INI file parser
   - Implement parameter tuning
   - Create preset system
   - Document: `sound-font-structure.md`

---

## Quick Reference

### Key Formulas

**Swing Speed:**
```
swing_speed = √(gyro_y² + gyro_z²)  // degrees/second
```

**Swing Strength:**
```
swing_strength = min(1.0, swing_speed / SwingSensitivity)
```

**Volume with Power Curve:**
```
mixhum = swing_strength ^ SwingSharpness
swing_volume = mixhum × MaxSwingVolume
hum_volume = 1.0 - mixhum × MaximumHumDucking / 100
```

**Virtual Rotation:**
```
position -= swing_speed × delta_time  // degrees
```

**Crossfade:**
```
mixab = clamp(-PlayerA.begin() / PlayerA.width, 0.0, 1.0)
PlayerA_volume = swing_volume × mixab
PlayerB_volume = swing_volume × (1.0 - mixab)
```

**Dynamic Compression:**
```
output = input × volume / (√running_average + 100)
```

### Default Parameters

| Parameter | Default | Range | Effect |
|-----------|---------|-------|--------|
| SwingSensitivity | 450 deg/s | 200-800 | Speed at max volume |
| SwingSharpness | 1.75 | 0.5-3.0 | Volume response curve |
| MaximumHumDucking | 75% | 0-100% | Hum reduction |
| SwingStrengthThreshold | 20 deg/s | 10-50 | Minimum trigger speed |
| MaxSwingVolume | 3.0 | 1.0-5.0 | Swing volume multiplier |
| Transition1Degrees | 45° | 20-90° | First transition width |
| Transition2Degrees | 160° | 90-240° | Second transition width |

### Hardware Requirements

**Minimum:**
- ESP32-C6 or ESP32-S3 (240 MHz)
- IMU with I2C (MPU6050 or LSM6DS3H)
- I2S DAC (MAX98357A recommended)
- SD card (Class 10 minimum)
- 8 MB flash (for firmware)

**Recommended:**
- ESP32-S3 (240 MHz, more RAM)
- LSM6DS3H (1600 Hz capable)
- PCM5102A or MAX98357A (I2S)
- UHS-I SD card (faster reads)
- 16 MB flash

### Software Requirements

**Core Libraries:**
- ESP-IDF I2C driver
- ESP-IDF I2S driver
- FreeRTOS tasks
- FAT filesystem (SD card)

**Optional:**
- INI parser (configuration)
- Logging/debugging tools

---

## Troubleshooting Guide

### Issue: SmoothSwing not triggering
**Check:**
1. Gyro sampling rate ≥ 400 Hz
2. SwingStrengthThreshold not too high (try 15)
3. Motion processing actually running
4. Swing speed calculation using Y/Z axes only

### Issue: Audio distortion/clipping
**Check:**
1. Dynamic compression enabled
2. MaxSwingVolume not too high (try 2.0)
3. Global volume reasonable (≤ 16384)
4. DMA buffer not underrunning

### Issue: Choppy/stuttering audio
**Check:**
1. SD card fast enough (Class 10+)
2. Audio buffers large enough (512 bytes)
3. File read task priority correct
4. Files not fragmented

### Issue: Abrupt transitions
**Check:**
1. Transition widths large enough (45°, 160°)
2. Fade times set (0.2s default)
3. Loop points in audio files seamless
4. VolumeOverlay working correctly

---

## Additional Resources

### ProffieOS Source Reference
- **Motion:** `/common/fuse.h`, `/motion/`
- **Audio:** `/sound/dac.h`, `/sound/dynamic_mixer.h`
- **SmoothSwing:** `/sound/smooth_swing_v2.h`
- **Config:** `/sound/smooth_swing_config.h`

### Useful Tools
- **Audio:** Audacity (loop points, normalization)
- **IMU Testing:** Arduino Serial Plotter
- **SD Card:** SD Card Formatter (optimize)
- **Debugging:** Logic analyzer (I2C/I2S)

### Community
- ProffieOS forum: http://crucible.hubbe.net
- ProffieOS documentation: https://pod.hubbe.net/

---

## Document Status

| Document | Status | Last Updated | Pages |
|----------|--------|--------------|-------|
| smoothswing-overview.md | ✓ Complete | 2026-04-06 | 8 |
| motion-processing.md | ✓ Complete | 2026-04-06 | 17 |
| audio-system.md | ✓ Complete | 2026-04-06 | 21 |
| smoothswing-algorithm.md | ✓ Complete | 2026-04-06 | 18 |
| audio-mixing-pipeline.md | ✓ Complete | 2026-04-06 | 18 |
| sound-font-structure.md | ✓ Complete | 2026-04-06 | 16 |
| **effects-catalog.md** | ✓ Complete | 2026-04-06 | 40 |
| **sound-effects-reference.md** | ✓ Complete | 2026-04-06 | 25 |
| **implementation-roadmap.md** | ✓ Complete | 2026-04-06 | 15 |

**Total:** ~180 pages of technical documentation

---

## License & Attribution

This documentation is based on reverse-engineering ProffieOS, which is GPL v3 licensed.

**Original Work:**
- ProffieOS: Fredrik Hubinette
- SmoothSwing Algorithm: Thexter

**This Documentation:**
- Created: 2026-04-06
- Purpose: Educational and implementation reference
- Target: ESP32 platforms (C6/S3)

**Note:** This documentation is intended to help you implement your own version. If you plan to use ProffieOS code directly, respect the GPL v3 license terms.

---

## Questions?

This documentation should provide everything needed to implement SmoothSwing V2 on ESP32. If you encounter issues:

1. **Check relevant document section** - use the table of contents
2. **Verify hardware connections** - I2C and I2S especially critical
3. **Test components individually** - motion, audio, mixing separately
4. **Refer to formulas section** - verify calculations match
5. **Compare with ProffieOS source** - when documentation unclear

Good luck with your lightsaber project! May your swings be smooth and your clashes epic! ⚔️
