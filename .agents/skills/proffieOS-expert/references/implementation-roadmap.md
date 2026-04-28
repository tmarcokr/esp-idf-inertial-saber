# ESP32 Lightsaber Implementation Roadmap

## Introduction

This document provides a practical, step-by-step implementation guide for building a ProffieOS-inspired lightsaber system on ESP32-C6/S3 hardware. The roadmap is organized into progressive phases, starting with basic motion effects and culminating in a fully-featured implementation with advanced battle mode intelligence.

### Prerequisites

**What You Already Have:**
- ✅ ESP32-C6 or ESP32-S3 development board
- ✅ I2S DAC working (hum playback confirmed)
- ✅ IMU sensor (I2C) connected and readable
- ✅ Basic audio playback system operational
- ✅ Development environment configured

**What You'll Build:**
- Motion-reactive sound effects (clash, swing, stab)
- SmoothSwing V2 algorithm implementation
- Advanced gesture detection (twist, shake)
- Lockup system with multiple modes
- Battle mode intelligence

### Estimated Timeline

| Phase | Duration | Difficulty | Result |
|-------|----------|------------|--------|
| Phase 1: Foundation | Complete | ⭐ | Audio + IMU working |
| Phase 2: Basic Motion | 1-2 weeks | ⭐⭐ | Clash + Swing detection |
| Phase 3: SmoothSwing | 1 week | ⭐⭐⭐ | Dynamic swing audio |
| Phase 4: Advanced Motion | 2-3 weeks | ⭐⭐⭐⭐ | Stab + Twist + Shake |
| Phase 5: Lockup System | 1-2 weeks | ⭐⭐⭐ | Blade-to-blade effects |
| Phase 6: Polish | 1-2 weeks | ⭐⭐⭐⭐ | Advanced features |

**Total: 6-10 weeks** (working part-time)

### Learning Curve

```
Complexity
    ^
    |                              ╱─── Advanced Motion
    |                         ╱───
    |                    ╱───       ╱─── Polish
    |               ╱───       ╱───
    |          ╱───       ╱───
    |     ╱───       ╱───
    |╱───       ╱───
    └────────────────────────────────> Time
     Phase 1-2  Phase 3  Phase 4-5  Phase 6
```

---

## Phase 1: Foundation (✓ Complete)

### Current Status

**Working:**
- ✅ I2S DAC configured and tested
- ✅ WAV file playback from SD card
- ✅ Hum loop playing continuously
- ✅ IMU sensor (accelerometer + gyroscope) accessible via I2C
- ✅ Basic RTOS tasks for audio and sensor reading

**Architecture:**
```
┌─────────────┐
│   SD Card   │──> WAV files (hum.wav, clash01.wav, etc.)
└──────┬──────┘
       │
       v
┌─────────────┐
│ Audio Task  │──> Read + buffer audio data
└──────┬──────┘
       │
       v
┌─────────────┐
│  I2S DMA    │──> Hardware audio output
└─────────────┘

┌─────────────┐
│  IMU (I2C)  │──> Accel (3-axis) + Gyro (3-axis)
└──────┬──────┘
       │
       v
┌─────────────┐
│ Motion Task │──> Read sensor @ 800 Hz
└─────────────┘
```

### What's Needed for Next Phase

**Hardware:**
- ✅ All hardware ready

**Software Components:**
1. **Motion processing pipeline** - Filter and process IMU data
2. **Event system** - Trigger sound effects based on motion
3. **Audio mixing** - Play multiple sounds simultaneously (hum + effects)
4. **Effect manager** - Queue and prioritize sound effects

**Memory Budget:**
- Current: ~6 KB RAM
- Target for Phase 2: ~8 KB RAM
- Headroom: ESP32 has 200+ KB available

---

## Phase 2: Basic Motion Effects (1-2 weeks)

**Goal:** Add clash and swing detection with reliable triggering

### 2.1 Clash Detection (⭐⭐)

**Complexity:** Moderate - Real-time filtering, dynamic thresholds

**Prerequisites:**
- Accelerometer reading at 800 Hz
- Basic understanding of vector mathematics
- Familiarity with digital filtering

#### Algorithm Overview

From `effects-catalog.md`, clash detection combines accelerometer and gyroscope data:

```
effective_threshold = CLASH_THRESHOLD_G × (1 + gyro_adjustment + audio_adjustment)

where:
  gyro_adjustment = gyro().len() / 500.0
  audio_adjustment = dynamic_mixer.audio_volume() × (suppression_level × 1E-10) × volume
```

Clash is detected when:
1. Sudden impact acceleration exceeds dynamic threshold
2. Combines filtered accelerometer data with gyroscope clash detection
3. Applies cooldown timer to prevent rapid re-triggering

#### Implementation Steps

**Step 1: Implement BoxFilter (effects-catalog.md, line 96-99)**

```cpp
// Box filter for clash detection (averages over N samples)
template<int N>
class BoxFilter {
private:
    float buffer[N];
    int index = 0;
    float sum = 0.0f;

public:
    void reset() {
        memset(buffer, 0, sizeof(buffer));
        index = 0;
        sum = 0.0f;
    }

    float filter(float value) {
        sum -= buffer[index];
        buffer[index] = value;
        sum += value;
        index = (index + 1) % N;
        return sum / N;
    }
};

// For clash detection, use 1600 Hz effective rate
BoxFilter<2> accel_clash_filter_x;  // Filter each axis separately
BoxFilter<2> accel_clash_filter_y;
BoxFilter<2> accel_clash_filter_z;
```

**Step 2: Calculate Clash Metrics**

```cpp
struct Vec3 {
    float x, y, z;

    float len() const {
        return sqrtf(x*x + y*y + z*z);
    }

    Vec3 operator-(const Vec3& other) const {
        return {x - other.x, y - other.y, z - other.z};
    }
};

// Gravity vector (fused estimate, updated continuously)
Vec3 down = {0.0f, 0.0f, -1.0f};  // Initial guess

// Clash acceleration (prop_base.h:325-327)
Vec3 get_clash_mss() {
    Vec3 filtered_accel;
    filtered_accel.x = accel_clash_filter_x.get();
    filtered_accel.y = accel_clash_filter_y.get();
    filtered_accel.z = accel_clash_filter_z.get();

    // Remove gravity component
    return filtered_accel - down;
}

// Gyro clash value (prop_base.h:330-344)
float get_gyro_clash_value(Vec3 current_gyro, Vec3 predicted_gyro) {
    Vec3 diff = current_gyro - predicted_gyro;
    float v = diff.len();
    return v / 9.81f;  // Convert to m/s² (assuming 1m blade)
}
```

**Step 3: Dynamic Threshold Calculation**

```cpp
// Configuration
const float CLASH_THRESHOLD_G = 3.0f;  // User-adjustable (1.0-8.0)
const float AUDIO_CLASH_SUPPRESSION_LEVEL = 10.0f;  // Range: 1-50

// Calculate dynamic threshold (prop_base.h:830-835)
float get_clash_threshold(float current_gyro_speed, float audio_volume) {
    float gyro_adjust = current_gyro_speed / 500.0f;
    float audio_adjust = audio_volume * (AUDIO_CLASH_SUPPRESSION_LEVEL * 1E-10f) * audio_volume;

    return CLASH_THRESHOLD_G * (1.0f + gyro_adjust + audio_adjust);
}
```

**Step 4: Clash Detection Logic**

```cpp
// Cooldown timer (prevent rapid re-triggering)
uint32_t last_clash_time_ms = 0;
const uint32_t CLASH_COOLDOWN_MS = 250;  // 250ms between clashes

void process_clash(Vec3 accel, Vec3 gyro, uint32_t timestamp_ms) {
    // Update filters
    Vec3 filtered_accel;
    filtered_accel.x = accel_clash_filter_x.filter(accel.x);
    filtered_accel.y = accel_clash_filter_y.filter(accel.y);
    filtered_accel.z = accel_clash_filter_z.filter(accel.z);

    // Calculate clash metrics
    Vec3 diff = get_clash_mss();
    float accel_clash = diff.len();
    float gyro_clash = get_gyro_clash_value(gyro, predicted_gyro);

    // Combined clash value (prop_base.h:813)
    float clash_value = (accel_clash + gyro_clash) / 2.0f;

    // Check threshold
    float threshold = get_clash_threshold(gyro.len(), get_audio_volume());

    if (clash_value > threshold) {
        // Check cooldown
        if (timestamp_ms - last_clash_time_ms > CLASH_COOLDOWN_MS) {
            trigger_clash_effect();
            last_clash_time_ms = timestamp_ms;
        }
    }
}

void trigger_clash_effect() {
    // Play random clash sound (clash01.wav, clash02.wav, etc.)
    play_sound_effect(EFFECT_CLASH);

    // Optional: Trigger visual effect (blade flash)
    trigger_blade_effect(EFFECT_CLASH);
}
```

**Step 5: Gravity Vector Fusion (Basic)**

For Phase 2, use a simple complementary filter:

```cpp
// Constants
const float GYRO_TO_RADS = M_PI / 180.0f;  // Convert deg/s to rad/s
const float ALPHA = 0.98f;  // Trust gyro 98%, accel 2%

void update_gravity_vector(Vec3 accel, Vec3 gyro, float delta_t) {
    // Normalize accelerometer reading
    float accel_len = accel.len();
    if (accel_len < 0.5f || accel_len > 1.5f) {
        // Accel unreliable (high movement), trust gyro only
        return;
    }

    Vec3 accel_normalized = {accel.x / accel_len,
                              accel.y / accel_len,
                              accel.z / accel_len};

    // Simple complementary filter
    down.x = ALPHA * down.x + (1.0f - ALPHA) * accel_normalized.x;
    down.y = ALPHA * down.y + (1.0f - ALPHA) * accel_normalized.y;
    down.z = ALPHA * down.z + (1.0f - ALPHA) * accel_normalized.z;

    // Re-normalize
    float len = down.len();
    down.x /= len;
    down.y /= len;
    down.z /= len;
}
```

#### Testing Procedures

**Unit Tests:**
1. **Filter Test:** Feed known values to BoxFilter, verify output
2. **Threshold Test:** Verify dynamic adjustment with different gyro/audio values
3. **Vector Math Test:** Verify Vec3 operations (length, subtraction)

**Integration Tests:**
1. **Static Test:** Shake device gently - should NOT trigger
2. **Tap Test:** Tap hilt firmly - SHOULD trigger once
3. **Rapid Tap Test:** Tap 5 times quickly - should trigger at most 2 times (cooldown)
4. **Swing Test:** Swing without impact - should NOT trigger
5. **Swing + Hit Test:** Swing and stop abruptly - SHOULD trigger

**Real-World Validation:**
```cpp
// Enable debug logging
void trigger_clash_effect() {
    Serial.printf("CLASH! value=%.2f threshold=%.2f time=%lu\n",
                  clash_value, threshold, millis());
    play_sound_effect(EFFECT_CLASH);
}

// Monitor false positives/negatives
// Adjust CLASH_THRESHOLD_G based on testing
```

#### Common Issues

| Symptom | Diagnosis | Solution |
|---------|-----------|----------|
| Too sensitive (false triggers) | Threshold too low | Increase CLASH_THRESHOLD_G to 4.0-5.0 |
| Not sensitive enough | Threshold too high | Decrease to 2.0-2.5 |
| Triggers during swings | Gyro adjustment too low | Increase gyro divisor to 300-400 |
| Rapid re-triggering | Cooldown too short | Increase to 500ms |
| Delayed detection | Filter size too large | Reduce BoxFilter size to 1 sample |

#### Deliverables

- [ ] BoxFilter class implemented and tested
- [ ] Vec3 math library working
- [ ] Gravity vector fusion running
- [ ] Clash detection triggering reliably
- [ ] Clash sound playing without glitches
- [ ] False positive rate < 5%
- [ ] False negative rate < 5%

---

### 2.2 Swing Detection (⭐)

**Complexity:** Simple - Hysteresis threshold on gyroscope magnitude

**Prerequisites:**
- Gyroscope reading at 800 Hz
- Basic state machine understanding

#### Algorithm Overview

From `effects-catalog.md` (line 443-456), swing uses simple hysteresis:

```
Start swinging:  speed > 250 deg/s
Stop swinging:   speed < 100 deg/s
```

Hysteresis (150 deg/s gap) prevents rapid on/off toggling.

#### Implementation Steps

**Step 1: Calculate Swing Speed**

```cpp
// Swing speed uses only Y and Z axes (perpendicular to blade)
// X-axis is twist (ignored for swing)
float get_swing_speed(Vec3 gyro) {
    return sqrtf(gyro.y * gyro.y + gyro.z * gyro.z);
}
```

**Step 2: Implement Hysteresis State Machine**

```cpp
// State
bool is_swinging = false;

// Thresholds (from prop_base.h:1044, 1048)
const float SWING_START_THRESHOLD = 250.0f;  // deg/s
const float SWING_STOP_THRESHOLD = 100.0f;   // deg/s

void process_swing(Vec3 gyro) {
    float swing_speed = get_swing_speed(gyro);

    if (!is_swinging && swing_speed > SWING_START_THRESHOLD) {
        // Start swing
        is_swinging = true;
        trigger_swing_effect();
    }

    if (is_swinging && swing_speed < SWING_STOP_THRESHOLD) {
        // Stop swing
        is_swinging = false;
    }
}

void trigger_swing_effect() {
    // Play random swing sound (swing01.wav, swing02.wav, etc.)
    play_sound_effect(EFFECT_SWING);
}
```

**Step 3: Audio Mixing**

For Phase 2, implement simple mixing:

```cpp
// Mix hum + one effect
void mix_audio(int16_t* output, size_t samples) {
    // Get hum samples
    int16_t hum_buffer[samples];
    read_hum_samples(hum_buffer, samples);

    // Get effect samples (if any playing)
    int16_t effect_buffer[samples];
    bool effect_playing = read_effect_samples(effect_buffer, samples);

    // Mix (simple addition with clipping)
    for (size_t i = 0; i < samples; i++) {
        int32_t mixed = hum_buffer[i] + (effect_playing ? effect_buffer[i] : 0);

        // Clip to prevent overflow
        if (mixed > 32767) mixed = 32767;
        if (mixed < -32768) mixed = -32768;

        output[i] = (int16_t)mixed;
    }
}
```

#### Testing Procedures

**Unit Tests:**
1. **Threshold Test:** Feed known gyro values, verify state transitions
2. **Hysteresis Test:** Oscillate around 100-250 deg/s, verify stable state

**Integration Tests:**
1. **Slow Rotation Test:** Rotate slowly (50 deg/s) - should NOT trigger
2. **Fast Swing Test:** Swing quickly (300+ deg/s) - SHOULD trigger once
3. **Sustained Swing Test:** Keep swinging - should trigger only once at start
4. **Stop/Start Test:** Swing, stop, swing again - should trigger twice

**Real-World Validation:**
```cpp
void trigger_swing_effect() {
    Serial.printf("SWING! speed=%.1f deg/s time=%lu\n", swing_speed, millis());
    play_sound_effect(EFFECT_SWING);
}
```

#### Common Issues

| Symptom | Diagnosis | Solution |
|---------|-----------|----------|
| Too many swing triggers | Start threshold too low | Increase to 300 deg/s |
| Swings not detected | Start threshold too high | Decrease to 200 deg/s |
| Swing stops too early | Stop threshold too high | Decrease to 80 deg/s |
| Swing continues too long | Stop threshold too low | Increase to 120 deg/s |

#### Deliverables

- [ ] Swing speed calculation working
- [ ] Hysteresis state machine implemented
- [ ] Swing sounds triggering on motion
- [ ] No rapid re-triggering (only once per swing)
- [ ] Audio mixing working (hum + swing play together)

---

### Phase 2 Summary

**Memory Usage:**
- BoxFilter (3 axes): ~24 bytes
- Clash state: ~20 bytes
- Swing state: ~4 bytes
- Audio mixing buffers: ~1 KB
- **Total:** ~1.1 KB (well within budget)

**CPU Usage (at 240 MHz):**
- Clash detection: ~50 μs per sample
- Swing detection: ~10 μs per sample
- Audio mixing: ~100 μs per buffer
- **Total:** <5% CPU at 800 Hz sampling

**Deliverables Checklist:**
- [ ] Clash detection working with <5% false positive rate
- [ ] Swing detection triggering reliably
- [ ] Sound effects playing without audio glitches
- [ ] Basic audio mixing (hum + 1 effect) functional
- [ ] Motion processing running at 800 Hz stable
- [ ] Documentation of threshold values used

---

## Phase 3: SmoothSwing V2 (1 week)

**Goal:** Implement dynamic swing audio that responds to motion speed and direction

**Complexity:** ⭐⭐⭐ - Virtual rotation model, crossfading, volume curves

### Prerequisites

- Phase 2 complete (basic motion detection working)
- Understanding of audio looping and crossfading
- Familiarity with the "virtual disc" model (see `smoothswing-algorithm.md`)

### Algorithm Overview

From `smoothswing-algorithm.md`, SmoothSwing V2 uses a **virtual rotating disc** model:

1. **Two Players:** PlayerA (low swing) and PlayerB (high swing) always loop
2. **Virtual Position:** Gyroscope drives rotation through 360° disc
3. **Crossfade Zones:** Transition between players based on angular position
4. **Volume Control:** Speed determines overall volume, position determines A/B mix

**Key Insight:** Audio files NEVER change pitch or speed - only volume/crossfade changes.

### Implementation Steps

**Step 1: Sound File Management**

```cpp
// Sound file pairs (swingl + swingh must have matching counts)
struct SwingPair {
    const char* low_file;   // e.g., "swingl01.wav"
    const char* high_file;  // e.g., "swingh01.wav"
};

SwingPair swing_pairs[] = {
    {"swingl01.wav", "swingh01.wav"},
    {"swingl02.wav", "swingh02.wav"},
    {"swingl03.wav", "swingh03.wav"}
};
const int num_swing_pairs = 3;

// Current active pair (only ONE pair loaded at a time)
int current_pair_index = 0;
```

**Step 2: Player Structure**

```cpp
struct SwingPlayer {
    // Audio player reference
    AudioPlayer* player;

    // Virtual disc position
    float midpoint;     // Current angular position (degrees, -180 to +180)
    float width;        // Transition zone width (degrees)
    float separation;   // Distance to next transition (degrees)

    // Current volume (0.0 to 1.0)
    float volume;

    // Helper methods
    float begin() { return midpoint - width / 2.0f; }
    float end() { return midpoint + width / 2.0f; }

    void rotate(float degrees) {
        midpoint += degrees;

        // Wrap to [-180, 180]
        while (midpoint > 180.0f) midpoint -= 360.0f;
        while (midpoint < -180.0f) midpoint += 360.0f;
    }
};

// Two players (always active, looping)
SwingPlayer player_a;
SwingPlayer player_b;
```

**Step 3: Configuration**

```cpp
// User-adjustable parameters (smoothsw.ini or hardcoded)
struct SmoothSwingConfig {
    float swing_sensitivity = 450.0f;          // deg/s at max volume
    float max_hum_ducking = 75.0f;             // Percentage (0-100)
    float swing_sharpness = 1.75f;             // Power curve exponent
    float swing_threshold = 20.0f;             // Minimum trigger speed (deg/s)
    float transition1_degrees = 45.0f;         // First transition width
    float transition2_degrees = 160.0f;        // Second transition width
    float low_to_high_separation = 180.0f;     // Angular distance low→high
    float high_to_low_separation = 180.0f;     // Angular distance high→low
    float max_swing_volume = 3.0f;             // Volume multiplier
} ss_config;
```

**Step 4: State Machine**

```cpp
enum SmoothSwingState {
    SS_OFF,    // Idle, waiting for motion (volumes at 0%)
    SS_ON,     // Active swinging (volumes controlled by motion)
    SS_OUT     // Fading out (waiting for volumes to reach 0%)
};

SmoothSwingState ss_state = SS_OFF;
```

**Step 5: Initialization**

```cpp
void smoothswing_activate() {
    // Configure separations
    player_a.separation = ss_config.low_to_high_separation;
    player_b.separation = ss_config.high_to_low_separation;

    // Pick random starting pair
    pick_random_swing_pair();

    ss_state = SS_OFF;
}

void pick_random_swing_pair() {
    // Stop current players (if any)
    if (player_a.player) player_a.player->stop();
    if (player_b.player) player_b.player->stop();

    // Select random pair
    current_pair_index = random(num_swing_pairs);

    // Load and start looping (at volume 0)
    player_a.player = start_looping(swing_pairs[current_pair_index].low_file);
    player_b.player = start_looping(swing_pairs[current_pair_index].high_file);

    // Random starting position (10-60 degrees)
    float offset = random(10.0f, 60.0f);
    player_a.midpoint = offset;
    player_a.width = ss_config.transition1_degrees;

    player_b.midpoint = player_a.midpoint + player_a.separation;
    player_b.width = ss_config.transition2_degrees;

    // Randomly swap A/B for variation
    if (random(2) == 1) {
        SwingPlayer temp = player_a;
        player_a = player_b;
        player_b = temp;
    }

    // Start silent
    player_a.volume = 0.0f;
    player_b.volume = 0.0f;
    set_player_volume(player_a.player, 0.0f);
    set_player_volume(player_b.player, 0.0f);
}
```

**Step 6: Motion Processing**

```cpp
void smoothswing_process(Vec3 gyro, float delta_time_s) {
    // Calculate swing speed (Y and Z axes only)
    float swing_speed = sqrtf(gyro.y * gyro.y + gyro.z * gyro.z);

    switch (ss_state) {
        case SS_OFF:
            // Check threshold
            if (swing_speed > ss_config.swing_threshold) {
                ss_state = SS_ON;
                // Fall through to SS_ON processing
            } else {
                // Keep silent
                player_a.volume = 0.0f;
                player_b.volume = 0.0f;
                return;
            }
            // NO BREAK - intentional fall-through

        case SS_ON: {
            // Calculate swing strength (0.0 to 1.0)
            float swing_strength = fminf(1.0f, swing_speed / ss_config.swing_sensitivity);

            // Check for end of swing (hysteresis: 90% of threshold)
            if (swing_speed < ss_config.swing_threshold * 0.9f) {
                ss_state = SS_OUT;
                return;
            }

            // Rotate virtual disc
            float rotation = -swing_speed * delta_time_s;  // Negative for direction
            player_a.rotate(rotation);
            player_b.rotate(rotation);

            // Check for transition completion
            while (player_a.end() < 0.0f) {
                // Player A completed its zone, reposition for next transition
                player_b.midpoint = player_a.midpoint + player_a.separation;

                // Swap players
                SwingPlayer temp = player_a;
                player_a = player_b;
                player_b = temp;
            }

            // Calculate crossfade mix (0.0 = full A, 1.0 = full B)
            float mixab = fmaxf(0.0f, fminf(1.0f, -player_a.begin() / player_a.width));

            // Apply power curve to swing strength
            float mixhum = powf(swing_strength, ss_config.swing_sharpness);

            // Calculate volumes
            float swing_vol = mixhum * ss_config.max_swing_volume;
            float hum_vol = 1.0f - (mixhum * ss_config.max_hum_ducking / 100.0f);

            player_a.volume = swing_vol * (1.0f - mixab);
            player_b.volume = swing_vol * mixab;

            // Apply volumes
            set_player_volume(player_a.player, player_a.volume);
            set_player_volume(player_b.player, player_b.volume);
            set_hum_volume(hum_vol);
            break;
        }

        case SS_OUT:
            // Wait for both volumes to reach zero
            if (player_a.volume == 0.0f && player_b.volume == 0.0f) {
                // Pick new random pair
                pick_random_swing_pair();
                ss_state = SS_OFF;
            }
            // Volumes fade naturally via audio system fade time
            break;
    }
}
```

**Step 7: Audio Mixing (3 Streams)**

```cpp
void mix_audio_smoothswing(int16_t* output, size_t samples) {
    // Get samples from all three streams
    int16_t hum_buffer[samples];
    int16_t swing_a_buffer[samples];
    int16_t swing_b_buffer[samples];

    read_hum_samples(hum_buffer, samples);
    player_a.player->read_samples(swing_a_buffer, samples);
    player_b.player->read_samples(swing_b_buffer, samples);

    // Mix all three
    for (size_t i = 0; i < samples; i++) {
        int32_t mixed = hum_buffer[i] + swing_a_buffer[i] + swing_b_buffer[i];

        // Clip
        if (mixed > 32767) mixed = 32767;
        if (mixed < -32768) mixed = -32768;

        output[i] = (int16_t)mixed;
    }
}
```

### Testing Procedures

**Unit Tests:**
1. **Rotation Test:** Feed known speeds, verify position updates correctly
2. **Wrap Test:** Verify position wraps at ±180°
3. **Crossfade Test:** Verify mixab calculation at various positions
4. **Power Curve Test:** Verify swing_strength → mixhum calculation

**Integration Tests:**
1. **Slow Swing Test:** Swing at 100 deg/s - hear gradual Low→High transition
2. **Fast Swing Test:** Swing at 400 deg/s - hear rapid Low↔High alternation
3. **Volume Test:** Verify hum ducks when swinging
4. **Pair Change Test:** Swing, stop, swing - should hear different sound
5. **Continuous Swing Test:** Swing for 10 seconds - sound should stay dynamic

**Real-World Validation:**
```cpp
// Enable debug logging
void smoothswing_process(Vec3 gyro, float delta_time_s) {
    // ... process logic ...

    if (ss_state == SS_ON) {
        Serial.printf("SS: speed=%.1f pos_a=%.1f vol_a=%.2f vol_b=%.2f\n",
                      swing_speed, player_a.midpoint, player_a.volume, player_b.volume);
    }
}
```

### Common Issues

| Symptom | Diagnosis | Solution |
|---------|-----------|----------|
| Sound too quiet | Max volume too low | Increase max_swing_volume to 4.0 |
| Sound too loud | Max volume too high | Decrease to 2.0 |
| Hum disappears | Ducking too high | Reduce max_hum_ducking to 50% |
| Not responsive | Sensitivity too high | Lower swing_sensitivity to 300 |
| Too responsive | Sensitivity too low | Increase to 600 |
| Abrupt transitions | Width too narrow | Increase transition degrees |

### Reference Documentation

- **Full Algorithm:** `/Users/thiago.marco/Desktop/Personal/repos/ProffieOS/docs/smoothswing-algorithm.md`
- **Sound Requirements:** `/Users/thiago.marco/Desktop/Personal/repos/ProffieOS/docs/sound-effects-reference.md` (lines 409-443)

### Deliverables

- [ ] Two audio players looping simultaneously (swingl + swingh)
- [ ] Virtual disc rotation working based on gyro
- [ ] Crossfading between Low/High based on position
- [ ] Volume responding to swing speed
- [ ] Hum ducking during swings
- [ ] Random pair selection on swing end
- [ ] Audio mixing working (hum + 2 swing streams)
- [ ] No audio glitches or buffer underruns
- [ ] Responsive feel (< 50ms latency)

---

## Phase 4: Advanced Motion (2-3 weeks)

**Goal:** Implement stab, twist, shake gestures, and blade angle tracking

### 4.1 Stab Detection (⭐⭐)

**Reference:** `effects-catalog.md` lines 236-268

**Algorithm:**
Stab is a specialized clash with additional constraints:

```cpp
// Stab conditions (prop_base.h:839-840)
bool is_stab = (diff.x < -2.0f * sqrtf(diff.y * diff.y + diff.z * diff.z)) &&
               (swing_speed < 150.0f);
```

**Implementation:**

```cpp
void process_stab(Vec3 accel_diff, float swing_speed) {
    // Check clash detection first
    if (!clash_detected) return;

    // Check stab-specific conditions
    float lateral_accel = sqrtf(accel_diff.y * accel_diff.y +
                                 accel_diff.z * accel_diff.z);
    float forward_accel = accel_diff.x;

    bool is_stab = (forward_accel < -2.0f * lateral_accel) &&
                   (swing_speed < 150.0f);

    if (is_stab) {
        trigger_effect(EFFECT_STAB);  // Fallback: clash.wav
    } else {
        trigger_effect(EFFECT_CLASH);
    }
}
```

**Testing:**
- Swing and hit: Should trigger clash
- Thrust forward and hit: Should trigger stab
- Verify swing_speed threshold prevents stab during swings

**Deliverable:**
- [ ] Stab detection distinguishes from clash
- [ ] stab.wav plays (or falls back to clash.wav)

---

### 4.2 Twist Gesture (⭐⭐⭐⭐)

**Reference:** `effects-catalog.md` lines 270-352

**Algorithm:**
Detect alternating left/right rotation strokes within timing windows.

**Complexity:** Complex - Gesture state machine, stroke history, timing

**Implementation:**

```cpp
enum StrokeType {
    TWIST_NONE,
    TWIST_LEFT,
    TWIST_RIGHT,
    TWIST_CLOSE  // End of stroke
};

struct Stroke {
    StrokeType type;
    uint32_t start_millis;
    uint32_t end_millis;

    uint32_t length() const { return end_millis - start_millis; }
};

// Stroke history (circular buffer, last 4 strokes)
Stroke stroke_history[4];
int stroke_index = 0;

// Thresholds (prop_base.h:977-979, 992-999)
const float TWIST_MIN_RATE = 200.0f;        // deg/s
const float TWIST_AXIS_DOMINANCE = 3.0f;    // X must be 3x Y/Z
const uint32_t TWIST_MIN_DURATION = 90;     // ms
const uint32_t TWIST_MAX_DURATION = 300;    // ms
const uint32_t TWIST_MAX_SEPARATION = 200;  // ms

void detect_twist_strokes(Vec3 gyro) {
    bool is_twisting = (fabsf(gyro.x) > TWIST_MIN_RATE) &&
                       (fabsf(gyro.x) > TWIST_AXIS_DOMINANCE * fabsf(gyro.y)) &&
                       (fabsf(gyro.x) > TWIST_AXIS_DOMINANCE * fabsf(gyro.z));

    if (is_twisting) {
        StrokeType direction = (gyro.x > 0) ? TWIST_RIGHT : TWIST_LEFT;
        record_stroke(direction);
    } else {
        record_stroke(TWIST_CLOSE);
    }
}

void record_stroke(StrokeType type) {
    static StrokeType current_stroke = TWIST_NONE;
    static uint32_t stroke_start = 0;

    uint32_t now = millis();

    if (type != TWIST_CLOSE && current_stroke == TWIST_NONE) {
        // Start new stroke
        current_stroke = type;
        stroke_start = now;
    } else if (type == TWIST_CLOSE && current_stroke != TWIST_NONE) {
        // End stroke
        Stroke new_stroke;
        new_stroke.type = current_stroke;
        new_stroke.start_millis = stroke_start;
        new_stroke.end_millis = now;

        stroke_history[stroke_index] = new_stroke;
        stroke_index = (stroke_index + 1) % 4;

        current_stroke = TWIST_NONE;

        // Check for twist pattern
        check_twist_pattern();
    }
}

void check_twist_pattern() {
    // Get last two strokes
    int idx1 = (stroke_index + 3) % 4;  // Most recent
    int idx2 = (stroke_index + 2) % 4;  // Second most recent

    Stroke& last = stroke_history[idx1];
    Stroke& prev = stroke_history[idx2];

    // Check for opposite directions
    if ((last.type == TWIST_LEFT && prev.type == TWIST_RIGHT) ||
        (last.type == TWIST_RIGHT && prev.type == TWIST_LEFT)) {

        // Check stroke durations
        if (last.length() >= TWIST_MIN_DURATION && last.length() <= TWIST_MAX_DURATION &&
            prev.length() >= TWIST_MIN_DURATION && prev.length() <= TWIST_MAX_DURATION) {

            // Check separation
            uint32_t separation = last.start_millis - prev.end_millis;
            if (separation < TWIST_MAX_SEPARATION) {
                trigger_effect(EFFECT_TWIST);
            }
        }
    }
}
```

**Testing:**
1. Twist left then right quickly - should trigger
2. Twist slowly - should NOT trigger (outside timing)
3. Twist while swinging - should NOT trigger (Y/Z dominance)

**Deliverable:**
- [ ] Twist gesture recognized
- [ ] Timing windows enforced
- [ ] False positive rate < 10%

---

### 4.3 Shake Detection (⭐⭐⭐)

**Reference:** `effects-catalog.md` lines 354-434

**Algorithm:**
Requires 5 alternating forward/backward thrust strokes.

**Implementation:** (Similar structure to twist, but uses accel_diff instead of gyro, and requires 5 strokes)

```cpp
// Thresholds (prop_base.h:1014-1030)
const float SHAKE_MAX_LATERAL = 4.0f;       // m/s² (Y²+Z² < 16)
const float SHAKE_FORWARD_MIN = 7.0f;       // m/s²
const float SHAKE_BACKWARD_MIN = -6.0f;     // m/s²
const float SHAKE_MAX_SWING = 150.0f;       // deg/s
const uint32_t SHAKE_MAX_SEPARATION = 250;  // ms

// Stroke detection (similar to twist, but 5 strokes required)
```

**Deliverable:**
- [ ] Shake gesture recognized (5 strokes)
- [ ] Lateral accel threshold enforced
- [ ] Swing speed check prevents false triggers

---

### 4.4 Blade Angle (⭐⭐)

**Reference:** `effects-catalog.md` lines 533-588

**Algorithm:**
Calculate blade angle from gravity vector:

```cpp
// Blade angle (fuse.h:233-239)
// PI/2 = straight up, -PI/2 = straight down
float get_blade_angle() {
    return atan2f(down.x, sqrtf(down.z * down.z + down.y * down.y));
}

// Map to 0-32768 range for style functions
int get_blade_angle_int() {
    float angle_rad = get_blade_angle();
    float v = (angle_rad + M_PI / 2.0f) * 32768.0f / M_PI;
    return (int)v;
}
```

**Usage:**
Can be used for visual effects (blade color changes based on angle) or lockup type selection (drag vs melt vs normal).

**Deliverable:**
- [ ] Blade angle calculation working
- [ ] Angle updates based on gravity vector
- [ ] Range: 0 (down) to 32768 (up)

---

### Phase 4 Summary

**Memory Usage:**
- Stroke history: ~64 bytes
- Gesture state machines: ~32 bytes
- **Total:** ~100 bytes additional

**Deliverables:**
- [ ] Stab detection distinguishes from clash
- [ ] Twist gesture recognized reliably
- [ ] Shake gesture recognized (5 strokes)
- [ ] Blade angle tracking working
- [ ] All gestures tested with < 10% false positive rate

---

## Phase 5: Lockup System (1-2 weeks)

**Goal:** Implement blade-to-blade lockup modes with begin/loop/end sequences

**Reference:** `effects-catalog.md` lines 661-722

### 5.1 Lockup States (⭐⭐⭐)

**Lockup Types:**

```cpp
enum LockupType {
    LOCKUP_NONE,              // No lockup
    LOCKUP_NORMAL,            // Blade-to-blade
    LOCKUP_DRAG,              // Dragging on ground
    LOCKUP_MELT,              // Melting through objects
    LOCKUP_LIGHTNING_BLOCK    // Force lightning block
};

LockupType current_lockup = LOCKUP_NONE;
```

**State Machine:**

```cpp
// Trigger: Clash + hold button (or gesture)
void start_lockup(LockupType type) {
    current_lockup = type;

    // Play begin sound
    play_sound_effect(get_begin_effect(type));  // bgnlock.wav, bgndrag.wav, etc.

    // Transition to loop after begin completes
    set_next_effect(get_loop_effect(type));     // lockup.wav, drag.wav, etc.
}

// Release button
void end_lockup() {
    LockupType type = current_lockup;
    current_lockup = LOCKUP_NONE;

    // Play end sound
    play_sound_effect(get_end_effect(type));    // endlock.wav, enddrag.wav, etc.
}

// Helper functions
EffectType get_begin_effect(LockupType type) {
    switch (type) {
        case LOCKUP_NORMAL: return EFFECT_BGNLOCK;
        case LOCKUP_DRAG: return EFFECT_BGNDRAG;
        case LOCKUP_MELT: return EFFECT_BGNMELT;
        case LOCKUP_LIGHTNING_BLOCK: return EFFECT_BGNLB;
        default: return EFFECT_BGNLOCK;
    }
}

EffectType get_loop_effect(LockupType type) {
    switch (type) {
        case LOCKUP_NORMAL: return EFFECT_LOCKUP;
        case LOCKUP_DRAG: return EFFECT_DRAG;
        case LOCKUP_MELT: return EFFECT_MELT;
        case LOCKUP_LIGHTNING_BLOCK: return EFFECT_LB;
        default: return EFFECT_LOCKUP;
    }
}

EffectType get_end_effect(LockupType type) {
    switch (type) {
        case LOCKUP_NORMAL: return EFFECT_ENDLOCK;
        case LOCKUP_DRAG: return EFFECT_ENDDRAG;
        case LOCKUP_MELT: return EFFECT_ENDMELT;
        case LOCKUP_LIGHTNING_BLOCK: return EFFECT_ENDLB;
        default: return EFFECT_ENDLOCK;
    }
}
```

**Deliverable:**
- [ ] Lockup state machine implemented
- [ ] Begin/loop/end sequence working
- [ ] Transition between states smooth

---

### 5.2 Lockup Type Selection (⭐⭐)

**Algorithm:**
Select lockup type based on blade angle:

```cpp
LockupType select_lockup_type() {
    float angle = get_blade_angle();

    // Angle ranges (approximate):
    // -PI/2 to -PI/4: Drag (blade pointing down)
    // -PI/4 to +PI/4: Normal (blade horizontal)
    // +PI/4 to +PI/2: Melt (blade pointing up)

    if (angle < -M_PI / 4.0f) {
        return LOCKUP_DRAG;
    } else if (angle > M_PI / 4.0f) {
        return LOCKUP_MELT;
    } else {
        return LOCKUP_NORMAL;
    }
}
```

**Trigger Mechanism:**
```cpp
// Button-based (simple)
void on_button_hold_during_clash() {
    LockupType type = select_lockup_type();
    start_lockup(type);
}

// Gesture-based (advanced)
void on_sustained_impact() {
    // If impact lasts > 200ms, auto-lockup
    LockupType type = select_lockup_type();
    start_lockup(type);
}
```

**Deliverable:**
- [ ] Lockup type selected based on angle
- [ ] Manual triggering (button) working
- [ ] Optional: Auto-lockup on sustained clash

---

### 5.3 Audio During Lockup (⭐⭐⭐)

**Behavior:**
- **Monophonic fonts:** Lockup replaces hum
- **Polyphonic fonts:** Lockup plays over hum

```cpp
void mix_audio_with_lockup(int16_t* output, size_t samples) {
    int16_t hum_buffer[samples];
    int16_t lockup_buffer[samples];

    read_hum_samples(hum_buffer, samples);
    bool lockup_playing = read_lockup_samples(lockup_buffer, samples);

    if (is_polyphonic_font) {
        // Polyphonic: Mix hum + lockup
        for (size_t i = 0; i < samples; i++) {
            int32_t mixed = hum_buffer[i] + (lockup_playing ? lockup_buffer[i] : 0);
            // Clip...
            output[i] = (int16_t)mixed;
        }
    } else {
        // Monophonic: Lockup replaces hum
        for (size_t i = 0; i < samples; i++) {
            output[i] = lockup_playing ? lockup_buffer[i] : hum_buffer[i];
        }
    }
}
```

**Deliverable:**
- [ ] Lockup audio playing correctly
- [ ] Monophonic mode: Lockup replaces hum
- [ ] Polyphonic mode: Lockup over hum
- [ ] Smooth transitions between states

---

### Phase 5 Summary

**Deliverables:**
- [ ] All 4 lockup types implemented (Normal, Drag, Melt, Lightning)
- [ ] Begin/loop/end sequences working
- [ ] Lockup type selected based on blade angle
- [ ] Audio mixing handles lockup correctly
- [ ] Button triggering working
- [ ] Visual effects synchronized with audio (optional)

---

## Phase 6: Polish & Advanced Features (1-2 weeks)

**Goal:** Add final polish and advanced effects

### 6.1 Multi-Blast Mode (⭐⭐⭐)

**Reference:** `effects-catalog.md` line 241

Play multiple blast sounds in rapid succession:

```cpp
void trigger_multi_blast(int count) {
    for (int i = 0; i < count; i++) {
        schedule_effect(EFFECT_BLASTER, i * 100);  // 100ms spacing
    }
}
```

**Deliverable:**
- [ ] Multiple blasts can play overlapped
- [ ] Audio mixing handles 3+ simultaneous effects

---

### 6.2 Force Effects (⭐⭐)

**Reference:** `effects-catalog.md` lines 449-455

Trigger force.wav on button press or gesture:

```cpp
void on_force_button() {
    play_sound_effect(EFFECT_FORCE);
    // Optional: Trigger visual pulse effect
}
```

**Deliverable:**
- [ ] Force effect triggerable
- [ ] Works with both monophonic and polyphonic fonts

---

### 6.3 Preon/Postoff (⭐⭐)

**Reference:** `sound-effects-reference.md` lines 189-204

Pre-ignition and post-retraction effects:

```cpp
void ignite_blade() {
    if (has_preon_sound()) {
        play_sound_effect(EFFECT_PREON);
        // Wait for preon to complete
        wait_for_effect_end();
    }

    play_sound_effect(EFFECT_POWERON);
    // Start hum
}

void retract_blade() {
    play_sound_effect(EFFECT_POWEROFF);

    if (has_postoff_sound()) {
        // Wait for poweroff to complete
        wait_for_effect_end();
        play_sound_effect(EFFECT_PSTOFF);
    }
}
```

**Deliverable:**
- [ ] Preon plays before ignition
- [ ] Postoff plays after retraction
- [ ] Timing synchronized correctly

---

### 6.4 Battle Mode Intelligence (⭐⭐⭐⭐⭐)

**Reference:** `effects-catalog.md` lines 779-786

Advanced features:
- Auto-lockup after clash (if button held)
- Clash intensity affects sound selection
- Swing-on ignition

```cpp
// Auto-lockup delay (Fett263: 200ms default)
const uint32_t AUTO_LOCKUP_DELAY = 200;
uint32_t last_clash_time = 0;

void on_clash() {
    last_clash_time = millis();
    play_clash_sound();
}

void check_auto_lockup() {
    if (button_held() &&
        (millis() - last_clash_time) > AUTO_LOCKUP_DELAY &&
        (millis() - last_clash_time) < AUTO_LOCKUP_DELAY + 50) {

        // Button still held 200ms after clash - enter lockup
        start_lockup(select_lockup_type());
    }
}
```

**Deliverable:**
- [ ] Auto-lockup on held button after clash
- [ ] Configurable delay
- [ ] Optional: Clash intensity selection

---

## Resource Requirements

### Per-Phase Memory Usage

| Phase | RAM (KB) | Flash (KB) | Notes |
|-------|----------|------------|-------|
| 1: Foundation | ~6 | ~50 | Audio + IMU base |
| 2: Basic Motion | ~8 | ~55 | + Clash/Swing detection |
| 3: SmoothSwing | ~10 | ~60 | + 2 swing players |
| 4: Advanced Motion | ~11 | ~65 | + Gesture detection |
| 5: Lockup | ~12 | ~70 | + Lockup states |
| 6: Polish | ~14 | ~75 | + Advanced features |

**ESP32-C6/S3 Available:**
- RAM: 200+ KB (plenty of headroom)
- Flash: 4+ MB (ample space)

### CPU Usage Estimates (at 240 MHz)

| Task | Frequency | CPU % | Notes |
|------|-----------|-------|-------|
| IMU Reading | 800 Hz | ~2% | I2C transfers |
| Motion Processing | 800 Hz | ~3% | Filtering + detection |
| Audio Mixing | 44.1 kHz | ~15% | 3-4 streams |
| SmoothSwing | 800 Hz | ~1% | Volume calculations |
| Gesture Detection | 800 Hz | <1% | Event-driven |
| **Total** | - | **~22%** | Plenty of headroom |

---

## Effect Dependencies

```
Foundation (Phase 1)
    ├─> IMU Sensor
    └─> Audio Playback
         │
         ├─> Basic Motion (Phase 2)
         │    ├─> Clash Detection
         │    └─> Swing Detection
         │         │
         │         └─> SmoothSwing (Phase 3)
         │              └─> Advanced Motion (Phase 4)
         │                   ├─> Stab
         │                   ├─> Twist
         │                   ├─> Shake
         │                   └─> Blade Angle
         │                        │
         │                        └─> Lockup System (Phase 5)
         │                             └─> Polish (Phase 6)
         │
         └─> Audio Mixing
              ├─> Hum + 1 Effect (Phase 2)
              ├─> Hum + 2 Swing (Phase 3)
              └─> Hum + 3+ Effects (Phase 6)
```

**Build Order Recommendation:**
1. Foundation first (audio + IMU)
2. Clash before Swing (more complex)
3. SmoothSwing after basic Swing
4. Gestures in any order (independent)
5. Lockup requires Blade Angle
6. Polish last (uses all previous)

---

## Testing Procedures

### Unit Testing Strategy

For each phase, create isolated tests:

```cpp
// Example: Clash detection unit test
void test_clash_detection() {
    // Setup
    reset_clash_state();
    Vec3 accel = {10.0f, 0.0f, 0.0f};  // Strong forward impact
    Vec3 gyro = {0.0f, 0.0f, 0.0f};    // No rotation

    // Execute
    process_clash(accel, gyro, millis());

    // Verify
    assert(was_clash_triggered());
    printf("✓ Clash detection unit test passed\n");
}
```

### Integration Testing Strategy

Test interactions between systems:

```cpp
// Example: SmoothSwing + Clash integration test
void test_smoothswing_clash_integration() {
    // Start swinging
    simulate_swing(300.0f);  // deg/s
    delay(500);

    // Clash during swing
    simulate_clash(5.0f);    // G

    // Verify
    assert(was_clash_triggered());
    assert(is_smoothswing_still_active());
    assert(audio_mixed_correctly());
    printf("✓ SmoothSwing+Clash integration test passed\n");
}
```

### Real-World Validation

Physical testing with actual device:

**Checklist per Phase:**
- [ ] No false positives in 5 minutes of normal handling
- [ ] >95% detection rate for intended gestures
- [ ] Audio glitch-free during all effects
- [ ] Battery life acceptable (measure current draw)
- [ ] Response time feels natural (<100ms latency)

---

## Troubleshooting Guide

### Common Issues by Phase

#### Phase 2: Basic Motion

| Symptom | Cause | Fix |
|---------|-------|-----|
| Audio glitches | Buffer underrun | Increase buffer size to 2048 samples |
| Clash too sensitive | Threshold too low | Increase CLASH_THRESHOLD_G to 4.0 |
| Swing not triggering | Threshold too high | Lower to 200 deg/s |
| IMU freezes | I2C bus lockup | Add I2C timeout + recovery |

#### Phase 3: SmoothSwing

| Symptom | Cause | Fix |
|---------|-------|-----|
| No crossfade | Players not looping | Verify loop flag set on WAV player |
| Abrupt transitions | Transition width too small | Increase to 60-90 degrees |
| CPU overload | Mixing too slow | Use DMA for I2S, optimize mixing loop |
| Wrong file playing | Pair mismatch | Verify swingl/swingh counts match |

#### Phase 4: Advanced Motion

| Symptom | Cause | Fix |
|---------|-------|-----|
| Twist false triggers | Threshold too low | Increase min rate to 250 deg/s |
| Shake won't trigger | Timing too strict | Increase max separation to 300ms |
| Stab misclassified | Swing check wrong | Verify swing_speed calculation |

#### Phase 5: Lockup

| Symptom | Cause | Fix |
|---------|-------|-----|
| Lockup won't exit | State machine stuck | Add timeout (max 30s lockup) |
| Wrong lockup type | Angle calc wrong | Debug gravity vector fusion |
| Audio cuts out | Mixing buffer overflow | Increase mixing buffer size |

---

## Configuration Management

### ESP32-Specific Considerations

**Storage Options:**
1. **NVS (Non-Volatile Storage):** Small key-value pairs (thresholds, config)
2. **SPIFFS:** Larger config files (smoothsw.ini, font metadata)
3. **SD Card:** Sound files, user presets

**Example Configuration Structure:**

```cpp
// config.h
struct MotionConfig {
    float clash_threshold_g = 3.0f;
    float swing_start_threshold = 250.0f;
    float swing_stop_threshold = 100.0f;
    uint32_t clash_cooldown_ms = 250;
};

struct SmoothSwingConfig {
    float swing_sensitivity = 450.0f;
    float max_hum_ducking = 75.0f;
    float swing_sharpness = 1.75f;
    float swing_threshold = 20.0f;
    // ... etc
};

// Load from NVS
void load_config() {
    nvs_handle_t handle;
    nvs_open("lightsaber", NVS_READONLY, &handle);

    size_t size = sizeof(MotionConfig);
    nvs_get_blob(handle, "motion", &motion_cfg, &size);

    size = sizeof(SmoothSwingConfig);
    nvs_get_blob(handle, "smoothswing", &ss_cfg, &size);

    nvs_close(handle);
}
```

### Per-Font Configuration

**Directory Structure:**
```
/sdcard/
  fonts/
    font1/
      hum.wav
      swingl01.wav
      swingh01.wav
      clash01.wav
      config.ini          <-- Font-specific settings
    font2/
      ...
  config/
    global.ini            <-- Global settings
```

**config.ini Parser:**
```cpp
// Simple INI parser
void parse_font_config(const char* font_path) {
    char ini_path[256];
    snprintf(ini_path, sizeof(ini_path), "%s/config.ini", font_path);

    File f = SD.open(ini_path);
    if (!f) return;  // Use defaults

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();

        if (line.startsWith("clash_threshold=")) {
            motion_cfg.clash_threshold_g = line.substring(16).toFloat();
        } else if (line.startsWith("swing_sensitivity=")) {
            ss_cfg.swing_sensitivity = line.substring(18).toFloat();
        }
        // ... parse other keys
    }

    f.close();
}
```

---

## Performance Optimization

### ESP32-Specific Tips

**1. Use DMA for I2S:**
```cpp
// Configure I2S with DMA
i2s_config_t i2s_config = {
    .mode = I2S_MODE_MASTER | I2S_MODE_TX,
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .dma_buf_count = 4,              // 4 DMA buffers
    .dma_buf_len = 512,              // 512 samples per buffer
    .use_apll = false,
    .tx_desc_auto_clear = true
};
```

**2. RTOS Task Priorities:**
```cpp
// High priority for audio (no glitches)
xTaskCreatePinnedToCore(audio_task, "audio", 4096, NULL,
                        configMAX_PRIORITIES - 1,  // Highest priority
                        &audio_task_handle, 1);    // Pin to core 1

// Medium priority for motion
xTaskCreatePinnedToCore(motion_task, "motion", 4096, NULL,
                        configMAX_PRIORITIES - 2,
                        &motion_task_handle, 0);   // Pin to core 0

// Low priority for UI
xTaskCreatePinnedToCore(ui_task, "ui", 2048, NULL, 1, &ui_task_handle, 0);
```

**3. Memory Allocation:**
```cpp
// Pre-allocate buffers (avoid heap fragmentation)
static int16_t audio_buffer_0[1024];
static int16_t audio_buffer_1[1024];
static int16_t mix_buffer[1024];

// Use DMA-capable memory for I2S
int16_t* dma_buffer = (int16_t*)heap_caps_malloc(2048, MALLOC_CAP_DMA);
```

**4. CPU Profiling:**
```cpp
// Measure task execution time
uint32_t start = esp_timer_get_time();
process_motion(accel, gyro);
uint32_t elapsed = esp_timer_get_time() - start;

if (elapsed > 1000) {  // > 1ms is slow
    Serial.printf("SLOW: motion processing took %lu us\n", elapsed);
}
```

**5. SD Card Performance:**
```cpp
// Use larger read buffers
File f = SD.open("swingl01.wav");
f.setBufferSize(4096);  // 4KB buffer (default is 512)

// Cache file positions for random access
static uint32_t file_positions[10];  // Cache first 10 files
```

---

## Next Steps After Completion

### Adding More Effects

Once core system is complete:

1. **Color Change:** Cycle through blade colors
2. **Volume Control:** Adjust sound volume on-the-fly
3. **Font Switching:** Change sound fonts without reboot
4. **Battle Mode 2.0:** Advanced clash intensity analysis
5. **Spin Detection:** Trigger special effect on 360° spin

### Custom Sound Design

**Tools:**
- Audacity (free) - Edit WAV files
- ProffieOS Sound Font Editor - Create matched swingl/swingh pairs

**Tips:**
- Keep WAV files 16-bit, 44.1 kHz
- Swingl/swingh should be 2-4 seconds for smooth looping
- Use crossfade loops (fade out matches fade in)

### Visual Effects (Blade Styles)

**Next Phase:** Implement LED blade control
- WS2812B addressable LEDs
- Blade styles (ignition, retraction, clash flash)
- Synchronized with audio effects

### Button Control Schemes

**Implement:**
- Single button (click, double-click, hold)
- Two button (power + aux)
- Three button (power + aux + aux2)
- Touch-sensitive controls

---

## Conclusion

This roadmap provides a structured path from basic audio playback to a fully-featured lightsaber system. Each phase builds on previous work, with clear deliverables and testing procedures.

**Key Success Factors:**
1. **Test frequently** - Don't move to next phase with bugs
2. **Keep it simple** - Start with minimal implementation, add complexity later
3. **Measure performance** - Profile CPU and memory usage regularly
4. **Document tuning** - Record threshold values that work for your hardware
5. **Backup often** - Commit to git after each working feature

**Estimated Total Time:** 6-10 weeks part-time

**Final Result:** A responsive, feature-rich lightsaber with motion-reactive sound effects, SmoothSwing, gesture detection, and lockup modes - all running efficiently on ESP32 hardware.

---

**Document Version:** 1.0
**Created:** 2026-04-06
**Based On:** ProffieOS documentation and source code analysis
**Target Platform:** ESP32-C6 / ESP32-S3
