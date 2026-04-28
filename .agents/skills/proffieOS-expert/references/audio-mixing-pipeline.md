# Audio Mixing Pipeline - Technical Specification

## Overview

The audio mixing pipeline combines multiple simultaneous sound sources (hum, swing, clash, effects) into a single output stream with dynamic range compression, volume control, and motion-reactive mixing.

## Mixer Architecture

### AudioDynamicMixer Implementation

**Core Structure:**
```cpp
template<int N>
class AudioDynamicMixer {
private:
    AudioStream* streams_[N];
    int volume_;          // Global volume (0-16384)
    uint32_t vol_;        // Running average for compression
    int16_t temp_buffer_[AUDIO_BUFFER_SIZE];

public:
    int read(int16_t* output, int samples);
    void set_volume(int vol);  // 14-bit precision
};
```

**Instantiation:**
```cpp
AudioDynamicMixer<9> dynamic_mixer;
// 9 inputs: 7 wav players + 1 beeper + 1 talkie
```

## Stream Combination Algorithm

### Step 1: Summing Phase

**Process:**
```pseudocode
FUNCTION read(output_buffer, num_samples):
    // Initialize accumulator
    FOR i FROM 0 TO num_samples-1:
        sum[i] = 0
    END FOR

    // Sum all active streams
    FOR stream_index FROM 0 TO N-1:
        IF streams[stream_index] IS NOT NULL:
            samples_read = streams[stream_index].read(temp_buffer, num_samples)

            FOR i FROM 0 TO samples_read-1:
                sum[i] += temp_buffer[i]
            END FOR
        END IF
    END FOR

    // Continue to compression phase...
END FUNCTION
```

**Key Points:**
- All streams read independently
- Simple integer addition (32-bit accumulator prevents overflow)
- Inactive streams skipped automatically

### Step 2: Dynamic Compression

**Algorithm:**
```pseudocode
// Running volume tracker (exponential moving average)
FOR i FROM 0 TO num_samples-1:
    // Update running average
    vol += ABS(sum[i])
    vol -= (vol + 255) >> 8  // Decay: multiply by ~0.996

    // Calculate compressed sample
    compressed = sum[i] * volume / (SQRT(vol) + 100)

    // Clamp to 16-bit range
    output[i] = CLAMP(compressed, -32768, 32767)
END FOR
```

**Compression Formula:**
```
output = input × global_volume / (√running_average + 100)
```

**Components:**
- **input:** Summed sample value
- **global_volume:** User volume setting (0-16384)
- **running_average:** Exponential moving average of absolute values
- **+100:** Prevents division by zero, provides minimum headroom

### Why Square Root Compression?

**Linear Division:** `output = input / average`
- Too aggressive
- Quiet sounds become too loud
- Pumping artifacts

**Square Root Division:** `output = input / √average`
- Gentler compression
- Preserves dynamic range better
- More natural sound

**Example:**

| Input Sum | Running Avg | √Avg | Divisor | Compression Ratio |
|-----------|-------------|------|---------|-------------------|
| 10000 | 1000 | 31.6 | 131.6 | 7.6% |
| 10000 | 10000 | 100.0 | 200.0 | 50% |
| 10000 | 30000 | 173.2 | 273.2 | 73% |

**Result:** Louder passages compress more, quieter passages less

## Volume Control System

### Volume Hierarchy

```
┌─────────────────────────┐
│   Global Mixer Volume   │  Master control
│   (dynamic_mixer)       │
└───────────┬─────────────┘
            │ 0-16384 (14-bit)
            ▼
    ┌───────────────┬───────────────┐
    │               │               │
┌───┴────┐  ┌───────┴──────┐  ┌────┴─────┐
│  Hum   │  │    Swing     │  │  Effects │
│ Player │  │   (A & B)    │  │ Players  │
└───┬────┘  └───────┬──────┘  └────┬─────┘
    │               │               │
    ▼               ▼               ▼
Per-layer volume (VolumeOverlay)
0-16384 (14-bit)
```

### Per-Layer Volume Control

**VolumeOverlay Implementation:**
```cpp
template<typename T>
class VolumeOverlay {
private:
    T* source_;
    int current_volume_;   // Current volume (smooth)
    int target_volume_;    // Target volume
    int fade_speed_;       // Change per sample

public:
    int read(int16_t* buffer, int samples) {
        int count = source_->read(buffer, samples);

        for (int i = 0; i < count; i++) {
            // Smooth fade
            if (current_volume_ < target_volume_) {
                current_volume_ = min(current_volume_ + fade_speed_, target_volume_);
            } else if (current_volume_ > target_volume_) {
                current_volume_ = max(current_volume_ - fade_speed_, target_volume_);
            }

            // Apply volume (14-bit precision)
            buffer[i] = (buffer[i] * current_volume_) >> 14;
        }

        return count;
    }

    void set_volume(int vol) {
        target_volume_ = clamp(vol, 0, 16384);
    }

    void set_fade_time(float seconds) {
        // Calculate steps needed for complete fade
        int total_samples = seconds * 44100;
        fade_speed_ = 16384 / total_samples;
    }
};
```

**Volume Precision:**
- 14-bit: 0-16384
- 16384 = 100% (no attenuation)
- 8192 = 50% (-6 dB)
- 4096 = 25% (-12 dB)
- 0 = silent

**Smooth Fading:**
```
FUNCTION set_fade_time(seconds):
    samples_needed = seconds × 44100
    fade_speed = 16384 / samples_needed
END FUNCTION

// Example: 0.2s fade
fade_speed = 16384 / (0.2 × 44100) = 1.85 per sample
total_fade_time = 16384 / 1.85 ≈ 8855 samples ≈ 0.2s
```

## Clipping Prevention

### Three-Stage Protection

**Stage 1: Dynamic Compression**
- Primary protection
- Automatic gain reduction based on content
- Prevents clipping from multiple loud sources

**Stage 2: Volume Clamping**
```cpp
int16_t clamp(int32_t value) {
    if (value > 32767) return 32767;
    if (value < -32768) return -32768;
    return value;
}
```

**Stage 3: Headroom Constant**
```cpp
divisor = sqrt(vol_) + 100;
```
- +100 provides ~40 dB headroom minimum
- Ensures output never exceeds ±32767 even with zero running average

### Running Average Volume Calculation

**Exponential Moving Average:**
```cpp
vol_ += abs(sample);
vol_ -= (vol_ + 255) >> 8;
```

**Equivalent to:**
```
vol = vol × 0.99609375 + abs(sample)
```

**Time Constant:**
- At 44100 Hz: τ ≈ 255 / 44100 ≈ 5.8 ms
- 63% change in ~5.8 ms
- Responds quickly to volume changes

**Why +255 in decay term?**
- Prevents rounding errors in integer arithmetic
- Ensures smooth decay even at low volumes

### Custom Square Root

**Fast Integer Square Root:**
```cpp
uint32_t my_sqrt(uint32_t v) {
    static uint32_t last_v = 0;
    static uint32_t last_ret = 0;

    // Newton's method with previous result as initial guess
    if (v == last_v) return last_ret;

    uint32_t r = last_ret;  // Start from last result
    if (r == 0) r = 1;

    for (int i = 0; i < 5; i++) {  // 5 iterations
        r = (r + v / r) >> 1;  // Newton iteration
    }

    last_v = v;
    last_ret = r;
    return r;
}
```

**Optimization:**
- Caches last result
- Uses it as initial guess for next calculation
- Converges faster (typically 2-3 iterations)
- Avoids expensive hardware sqrt

## Motion Integration with Mixing

### Hum Volume Modulation

**Light Movement Enhancement:**
```cpp
float hum_volume = 1.0;

if (!swinging && swing_speed > 0) {
    float modifier = clamp(swing_speed / 200.0, 0.0, 2.3);
    hum_volume *= (0.99 + modifier);
}

hum_player->set_volume(hum_volume * 16384);
```

**Effect:**
- Idle (0 deg/s): 99% hum volume
- Light movement (200 deg/s): 199% hum volume
- Fast movement (460+ deg/s): 329% hum volume

**Purpose:** Creates more dynamic idle sound, blade feels "alive"

### Swing Volume Ducking

**SmoothSwing V2 Ducking:**
```cpp
// Calculate swing strength (0.0 to 1.0)
float swing_strength = min(1.0, swing_speed / SwingSensitivity);

// Apply power curve
float mixhum = pow(swing_strength, SwingSharpness);

// Calculate volumes
float swing_volume = mixhum * MaxSwingVolume;
float hum_volume = 1.0 - (mixhum * MaximumHumDucking / 100.0);

// Apply to players
swing_player_A->set_volume(swing_volume * crossfade_A * 16384);
swing_player_B->set_volume(swing_volume * crossfade_B * 16384);
hum_player->set_volume(hum_volume * 16384);
```

**Volume Relationship Graph:**
```
Volume
  ^
  │
  │   ╱╱╱╱╱╱╱╱╱╱╱ Swing (max: 3.0x)
3 │  ╱
  │ ╱
2 │╱
  │
1 │────╲╲╲╲╲╲╲╲╲ Hum (ducked to 0.25x)
  │
0 └────────────────────────────────→ Swing Speed
  0   SwingThreshold  SwingSensitivity
```

**Key Parameters:**
- **MaxSwingVolume:** 3.0 (swing can be 3× louder than hum)
- **MaximumHumDucking:** 75% (hum reduced to 25% at max swing)
- **SwingSharpness:** 1.75 (power curve for responsive feel)

### Accelerometer Effect on Pipeline

**Clash Detection Impact:**
```cpp
// During clash detection
if (clash_detected) {
    // Temporarily reduce clash sensitivity
    float audio_factor = dynamic_mixer.audio_volume()
                       * AUDIO_CLASH_SUPPRESSION_LEVEL * 1e-10
                       * dynamic_mixer.get_volume();

    clash_threshold += audio_factor;
}
```

**Purpose:** Prevents loud sounds (especially low frequencies) from triggering false clashes via speaker vibration

**Audio Clash Suppression:**
- Default level: 10
- Increases threshold by ~1g per 10,000 units of audio volume
- Automatic adaptation to current sound levels

## Polyphonic vs Monophonic Handling

### Detection Logic

**Font Type Auto-Detection:**
```pseudocode
FUNCTION detect_font_type():
    IF has_files("in") OR has_files("out"):
        RETURN POLYPHONIC_NEC
    ELSE IF has_files("poweron") OR has_files("poweroff"):
        IF has_files("humm"):
            RETURN POLYPHONIC_PLECTER
        ELSE:
            RETURN MONOPHONIC
        END IF
    END IF
END FUNCTION
```

### Polyphonic Mixing

**Characteristics:**
- Multiple sounds play simultaneously
- Each effect gets its own player
- No crossfading needed
- Hum runs continuously in dedicated player

**Player Allocation:**
```
Player 0: Hum (continuous)
Player 1: Swing A (SmoothSwing)
Player 2: Swing B (SmoothSwing)
Player 3: Clash/Effect
Player 4: Blast/Effect
Player 5: Lockup loop
Player 6: Menu sounds
Beeper: Simple tones
Talkie: Speech synthesis
```

**Mixing Strategy:**
```cpp
void PlayPolyphonic(Effect* effect) {
    // Get free or least-important player
    BufferedWavPlayer* player = GetFreePlayer();

    // Play on dedicated channel
    player->play(effect->random_file());
    player->set_volume(effect->volume());

    // No crossfade, plays independently
}
```

### Monophonic Mixing

**Characteristics:**
- Only one sound plays at a time
- Effects interrupt hum
- Requires crossfading
- Reuses same player

**Player Reuse:**
```
Player 0: Hum/Effects (shared)
Player 1: Swing A (SmoothSwing)
Player 2: Swing B (SmoothSwing)
Players 3-6: Additional effects if needed
```

**Mixing Strategy:**
```cpp
void PlayMonophonic(Effect* effect, float fade_time) {
    if (hum_player->is_playing()) {
        // Fade out hum
        hum_player->set_fade_time(fade_time);
        hum_player->FadeAndStop();

        // Wait for fade completion
        while (hum_player->volume() > 0) {
            delay_microseconds(100);
        }
    }

    // Reuse player for new effect
    hum_player->play(effect->random_file());
    hum_player->set_fade_time(fade_time);
    hum_player->set_volume(16384);  // Fade in
}
```

**Crossfade Duration:**
- Default: 3 ms (very fast)
- Prevents clicking
- Barely noticeable gap

### Hybrid Approach

Some fonts use hybrid technique:
- Hum plays continuously (polyphonic)
- Swings use monophonic crossfade
- Clashes play polyphonically

**Implementation:**
```cpp
if (is_swing_effect) {
    PlayMonophonic(effect, 0.003);  // 3ms crossfade
} else {
    PlayPolyphonic(effect);  // Independent playback
}
```

## Effect Prioritization

### Player Allocation Strategy

**Priority Order (high to low):**
1. **Hum:** Never killed, always playing when saber on
2. **SmoothSwing Players:** Protected during swinging
3. **Lockup loops:** Protected while held (reference counted)
4. **Clash/Blast:** Marked "killable" after minimum play time
5. **Menu sounds:** Shortest, most interruptible

**Allocation Algorithm:**
```pseudocode
FUNCTION GetPlayer(effect):
    // Try to find free player
    FOR i FROM 0 TO NUM_PLAYERS-1:
        IF players[i].is_idle():
            RETURN players[i]
        END IF
    END FOR

    // No free player, find one to interrupt
    IF KILL_OLD_PLAYERS enabled:
        best_candidate = NULL
        shortest_remaining = INFINITY

        FOR i FROM 0 TO NUM_PLAYERS-1:
            IF players[i].is_killable():
                remaining = players[i].remaining_time()
                IF remaining < shortest_remaining:
                    shortest_remaining = remaining
                    best_candidate = players[i]
                END IF
            END IF
        END FOR

        IF best_candidate:
            best_candidate.FadeAndStop(0.001)  // 1ms fade
            wait_for_silence(best_candidate)
            RETURN best_candidate
        END IF
    END IF

    ERROR("No available players")
END FUNCTION
```

### Swing Overlap Control

**Prevents Too-Frequent Swings:**
```cpp
// Check if current swing has played long enough
float progress = swing_player->position() / swing_player->length();

if (progress >= ProffieOSSwingOverlap) {
    // Allow new swing
    swing_player->FadeAndStop();
    PlayNewSwing();
} else {
    // Ignore new swing request
}
```

**ProffieOSSwingOverlap:**
- Default: 0.5 (50%)
- Range: 0.0 (no overlap) to 1.0 (full playback required)
- Prevents swing sound spam

### Effect Chaining

**Following Effects:**
```cpp
struct Effect {
    Effect* following_;  // Auto-triggered after this effect ends

    void OnComplete() {
        if (following_) {
            PlayEffect(following_);
        }
    }
};
```

**Example Chains:**
- preon → poweron → hum
- poweroff → postoff
- lockup_begin → lockup_loop → lockup_end

### Reference Counting

**Protected Playback:**
```cpp
class BufferedWavPlayer {
private:
    int refs_;  // Reference count

public:
    bool is_killable() {
        return refs_ == 0 && !is_protected();
    }

    void AddRef() { refs_++; }
    void Release() { refs_--; }
};
```

**Usage:**
```cpp
// Start lockup loop
BufferedWavPlayer* lockup = PlayEffect(SFX_lockup);
lockup->AddRef();  // Protect from interruption

// User releases lockup button
lockup->Release();  // Now interruptible
```

## Pipeline Performance

### CPU Usage (per audio callback)

**At 44.1 kHz, 44 samples/buffer (1 ms):**

| Stage | CPU Time | Percentage |
|-------|----------|------------|
| Read 7 WAV players | ~200 μs | 20% |
| Sum streams | ~20 μs | 2% |
| Compression calculation | ~100 μs | 10% |
| Square root | ~30 μs | 3% |
| Volume application | ~50 μs | 5% |
| **Total** | **~400 μs** | **40%** |

**At 240 MHz ESP32:** Leaves 60% CPU for other tasks

### Memory Usage

| Component | Size | Count | Total |
|-----------|------|-------|-------|
| DMA buffer | 176 bytes | 1 | 176 B |
| Player buffers | 512 bytes | 7 | 3.5 KB |
| Mixer temp buffer | 88 bytes | 1 | 88 B |
| File read buffer | 512 bytes | 1 | 512 B |
| Player objects | ~200 bytes | 7 | 1.4 KB |
| **Total** | | | **~5.7 KB** |

### Optimization Techniques

**1. Buffer Reuse:**
```cpp
// Single temp buffer for all reads
int16_t temp_buffer_[AUDIO_BUFFER_SIZE];

for (int i = 0; i < N; i++) {
    streams_[i]->read(temp_buffer_, samples);  // Reuse buffer
    // Process immediately
}
```

**2. Lazy Evaluation:**
```cpp
// Only calculate sqrt when needed
if (last_v != current_v) {
    result = my_sqrt(current_v);
    last_v = current_v;
}
```

**3. Integer Math:**
```cpp
// Use bit shifts instead of division
volume_applied = (sample * volume) >> 14;  // Divide by 16384
```

**4. Early Exit:**
```cpp
// Skip silent streams
if (stream->volume() == 0) continue;
```

## Underflow Detection

**Monitoring:**
```cpp
atomic<uint32_t> underflow_count_;

int read(int16_t* buffer, int samples) {
    int actual = mixer.read(buffer, samples);

    if (actual < samples) {
        underflow_count_++;
        // Pad with zeros
        memset(&buffer[actual], 0, (samples - actual) * 2);
    }

    return samples;
}
```

**Causes:**
- SD card too slow
- File fragmentation
- Insufficient buffering
- CPU overload

**Solutions:**
- Increase buffer sizes
- Use faster SD card (Class 10, UHS)
- Defragment files
- Optimize other tasks

## Complete Pipeline Pseudocode

```cpp
// Main audio callback (called ~1000 times/second)
void audio_callback() {
    int16_t output[AUDIO_BUFFER_SIZE];
    int32_t sum[AUDIO_BUFFER_SIZE] = {0};
    int16_t temp[AUDIO_BUFFER_SIZE];

    // Phase 1: Sum all streams
    for (int s = 0; s < NUM_STREAMS; s++) {
        if (streams[s] && streams[s]->volume() > 0) {
            int count = streams[s]->read(temp, AUDIO_BUFFER_SIZE);

            for (int i = 0; i < count; i++) {
                sum[i] += temp[i];
            }
        }
    }

    // Phase 2: Dynamic compression
    for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
        // Update running average
        vol += abs(sum[i]);
        vol -= (vol + 255) >> 8;

        // Compress
        int divisor = my_sqrt(vol) + 100;
        int32_t compressed = (sum[i] * global_volume) / divisor;

        // Clamp
        output[i] = clamp(compressed, -32768, 32767);
    }

    // Phase 3: Send to DAC
    i2s_write(I2S_NUM_0, output, sizeof(output), &bytes_written, portMAX_DELAY);
}
```

## Summary

The audio mixing pipeline provides:
- **Dynamic range compression** for clipping prevention
- **Per-layer volume control** with smooth fading
- **Motion-reactive mixing** (hum ducking, swing emphasis)
- **Polyphonic/monophonic support** with automatic detection
- **Effect prioritization** with intelligent player allocation
- **40% CPU usage** on 240 MHz ESP32
- **~6 KB RAM** total footprint

This architecture enables rich, responsive lightsaber sound with multiple simultaneous effects while maintaining audio quality and preventing distortion.
