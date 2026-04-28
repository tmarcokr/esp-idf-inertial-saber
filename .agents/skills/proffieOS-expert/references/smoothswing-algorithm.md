# SmoothSwing V2 Algorithm - Detailed Specification

## Algorithm Overview

SmoothSwing V2 uses a **virtual rotation model** where swing audio files are treated as positions on a rotating wheel. The gyroscope drives the rotation speed, and transition zones trigger crossfades between different swing sounds.

## Core Data Structures

### Swing Player Data

Each of the two players (A and B) maintains:

```cpp
struct SwingData {
    BufferedWavPlayer* player;  // Audio player reference
    float midpoint;             // Current angular position (degrees)
    float width;                // Transition zone width (degrees)
    float separation;           // Distance to next transition (degrees)

    // Helper methods
    float begin() {
        return midpoint - width / 2.0;
    }

    float end() {
        return midpoint + width / 2.0;
    }

    void rotate(float degrees) {
        midpoint += degrees;
        // Wrap to [-180, 180]
        while (midpoint > 180.0) midpoint -= 360.0;
        while (midpoint < -180.0) midpoint += 360.0;
    }
};
```

### State Machine

```cpp
enum SwingState {
    OFF,    // Idle, waiting for motion
    ON,     // Active swinging
    OUT     // Fading out
};
```

## Configuration Parameters

### User-Configurable Values

```cpp
struct SmoothSwingConfig {
    float SwingSensitivity = 450.0;              // deg/s at max volume
    float MaximumHumDucking = 75.0;              // Percentage (0-100)
    float SwingSharpness = 1.75;                 // Power curve exponent
    float SwingStrengthThreshold = 20.0;         // Minimum trigger speed (deg/s)
    float Transition1Degrees = 45.0;             // First transition width
    float Transition2Degrees = 160.0;            // Second transition width
    float Low2HighSeparationDegrees = 180.0;     // Angular distance low→high
    float High2LowSeparationDegrees = 180.0;     // Angular distance high→low
    float MaxSwingVolume = 3.0;                  // Volume multiplier
    float AccentSwingSpeedThreshold = 0.0;       // Speed for accent swings (0=off)
    float AccentSlashAccelerationThreshold = 260.0;  // Accel for slashes (deg/s²)
};
```

### Configuration File

Users can create `smoothsw.ini` in font directory:

```ini
Version=2
SwingSensitivity=450.0
MaximumHumDucking=75.0
SwingSharpness=1.75
SwingStrengthThreshold=20.0
Transition1Degrees=45.0
Transition2Degrees=160.0
Low2HighSeparationDegrees=180.0
High2LowSeparationDegrees=180.0
MaxSwingVolume=3.0
AccentSwingSpeedThreshold=0.0
AccentSlashAccelerationThreshold=260.0
```

## Algorithm Flow

### Phase 1: Initialization

**On Saber Activation:**

```pseudocode
FUNCTION Activate():
    // Load sound files
    swingl_files = scan_directory("swingl")  // Low swings
    swingh_files = scan_directory("swingh")  // High swings

    IF swingl_files.count == 0:
        swingl_files = scan_directory("lswing")  // Fallback
        swingh_files = scan_directory("hswing")
    END IF

    IF swingl_files.count != swingh_files.count:
        ERROR("Low and high swing file counts must match")
    END IF

    // Check for accent swings
    accent_swings = scan_directory("swng") OR scan_directory("swing")
    accent_slashes = scan_directory("slsh")

    // Configure separations
    PlayerA.separation = config.Low2HighSeparationDegrees
    PlayerB.separation = config.High2LowSeparationDegrees

    // Start with random pair
    PickRandomSwing()

    state = OFF
END FUNCTION
```

### Phase 2: Random Swing Selection

**Selecting Initial/Next Swing Pair:**

```pseudocode
FUNCTION PickRandomSwing():
    // Select random matching pair
    pair_index = random(0, swingl_files.count - 1)

    low_file = swingl_files[pair_index]
    high_file = swingh_files[pair_index]

    // Start players at random offset
    random_offset = random(10.0, 60.0)  // degrees

    PlayerA.play(low_file)
    PlayerB.play(high_file)

    // Set angular positions
    PlayerA.midpoint = random_offset
    PlayerA.width = config.Transition1Degrees

    PlayerB.midpoint = PlayerA.midpoint + PlayerA.separation
    PlayerB.width = config.Transition2Degrees

    // Randomly swap A/B for variation
    IF random(0, 1) == 1:
        SWAP(PlayerA, PlayerB)
    END IF

    // Optional: Sync to hum position
    IF hum_player.has_position():
        offset = hum_player.position_degrees()
        PlayerA.midpoint += offset
        PlayerB.midpoint += offset
    END IF

    // Start at zero volume (will fade in during motion)
    PlayerA.set_volume(0.0)
    PlayerB.set_volume(0.0)
END FUNCTION
```

## Understanding Swing File Management

### Key Concept: Only ONE Pair Active at a Time

**Important:** The system does NOT load all swing files simultaneously. Only ONE pair (swingl + swingh) is loaded and playing in loop at any given moment.

```
Available on SD Card:
  Pair 1: swingl01.wav ↔ swingh01.wav
  Pair 2: swingl02.wav ↔ swingh02.wav
  Pair 3: swingl03.wav ↔ swingh03.wav

Currently Active in Memory:
  Player A: swingl02.wav (looping at volume 0-100%)
  Player B: swingh02.wav (looping at volume 0-100%)
  Hum:      hum.wav      (looping, volume adjusted)

  → Pairs 1 and 3 are NOT loaded
```

### Memory Layout

```
┌─────────────────────────────────────────┐
│      ACTIVE IN MEMORY (Looping)        │
├─────────────────────────────────────────┤
│  Player A: swingl02.wav                 │
│    Buffer: 512 bytes                    │
│    Status: Loop, Volume: 75%            │
│                                         │
│  Player B: swingh02.wav                 │
│    Buffer: 512 bytes                    │
│    Status: Loop, Volume: 25%            │
│                                         │
│  Hum Player: hum.wav                    │
│    Buffer: 512 bytes                    │
│    Status: Loop, Volume: 50%            │
└─────────────────────────────────────────┘
Total RAM: ~1.5 KB

┌─────────────────────────────────────────┐
│      ON SD CARD (Not Loaded)            │
├─────────────────────────────────────────┤
│  swingl01.wav  💾 (~250 KB)            │
│  swingh01.wav  💾 (~250 KB)            │
│  swingl03.wav  💾 (~250 KB)            │
│  swingh03.wav  💾 (~250 KB)            │
│  ...and other files                     │
└─────────────────────────────────────────┘
```

### Lifecycle of Swing Pairs

```
Timeline:

t=0s: Saber Powers On
      ┌──────────────────────────┐
      │ PickRandomSwing()        │
      │ → Selects Pair 2         │
      │ → Loads swingl02.wav     │
      │ → Loads swingh02.wav     │
      │ → Both start looping     │
      │ → Volume = 0%            │
      └──────────────────────────┘
      State: OFF (waiting for motion)

t=1s: User Starts Swinging
      ┌──────────────────────────┐
      │ Motion detected          │
      │ → Volumes fade in        │
      │ → Pair 2 audible         │
      └──────────────────────────┘
      State: ON (active swinging)

t=5s: User Still Swinging
      ┌──────────────────────────┐
      │ Continuous sound         │
      │ → Still using Pair 2     │
      │ → Crossfading A ↔ B      │
      └──────────────────────────┘
      State: ON

t=8s: User Stops Moving
      ┌──────────────────────────┐
      │ Speed drops below 18°/s  │
      │ → Volumes fade out       │
      └──────────────────────────┘
      State: OUT (fading)

t=8.5s: Volumes Reach Zero
      ┌──────────────────────────┐
      │ BOTH volumes = 0%        │
      │ PickRandomSwing()        │
      │ → STOP Pair 2            │
      │ → SELECT Pair 1          │
      │ → LOAD swingl01.wav      │
      │ → LOAD swingh01.wav      │
      │ → START looping          │
      │ → Volume = 0%            │
      └──────────────────────────┘
      State: OFF (ready for next swing)

t=10s: User Swings Again
      ┌──────────────────────────┐
      │ Motion detected          │
      │ → Now using Pair 1!      │
      │ → Volumes fade in        │
      └──────────────────────────┘
      State: ON (different sound!)
```

### When Do Pairs Change?

**Trigger: Both player volumes reach 0%**

```pseudocode
STATE: OUT
  // Continuously check volumes
  IF PlayerA.volume == 0.0 AND PlayerB.volume == 0.0:
    // Stop current pair
    PlayerA.stop()
    PlayerB.stop()

    // Select new random pair
    new_pair = random(num_pairs)

    // Load and start new pair
    PlayerA.play(swingl_files[new_pair])
    PlayerB.play(swingh_files[new_pair])
    PlayerA.set_volume(0.0)
    PlayerB.set_volume(0.0)

    state = OFF
  END IF
```

**Key Points:**
- Pairs ONLY change when swing completely stops
- Never changes mid-swing
- New pair randomly selected
- Provides natural variation

### Memory Efficiency Comparison

**One Pair at a Time (Actual Implementation):**
```
Active Buffers:
  - swingl: 512 bytes
  - swingh: 512 bytes
  - hum:    512 bytes
  Total:    1.5 KB

CPU Load:
  - 3 audio streams to mix
  - 2 SD card readers active
```

**All Pairs Simultaneously (Hypothetical):**
```
Active Buffers (3 pairs):
  - swingl × 3: 1.5 KB
  - swingh × 3: 1.5 KB
  - hum:        0.5 KB
  Total:        3.5 KB

CPU Load:
  - 7 audio streams to mix
  - 6 SD card readers active
```

**Savings: 57% less RAM, 67% fewer audio streams**

### Example: Complete Cycle

```
You have 3 pairs: 01, 02, 03

Session Start:
  Random selection → Pair 02
  PlayerA: swingl02.wav (loop, vol=0)
  PlayerB: swingh02.wav (loop, vol=0)

First Swing (5 seconds):
  Motion → Volumes: 0% → 100% → 100% → 0%
  Sound: Pair 02 playing

Swing Ends:
  Volumes = 0%
  Random selection → Pair 03
  PlayerA: swingl03.wav (loop, vol=0)
  PlayerB: swingh03.wav (loop, vol=0)

Second Swing (4 seconds):
  Motion → Volumes: 0% → 100% → 100% → 0%
  Sound: Pair 03 playing (different than before!)

Swing Ends:
  Volumes = 0%
  Random selection → Pair 01
  PlayerA: swingl01.wav (loop, vol=0)
  PlayerB: swingh01.wav (loop, vol=0)

Third Swing (6 seconds):
  Motion → Volumes: 0% → 100% → 100% → 0%
  Sound: Pair 01 playing

Result: Each swing sounds different, using only 1.5 KB RAM
```

## Understanding Low vs High Swings

### Common Misconception

❌ **WRONG:** "Low swing = slow movement, High swing = fast movement"

✅ **CORRECT:** "Low and High refer to swing DIRECTION, not speed"

### What Low and High Actually Mean

```
        High Swing Zone
             ↑
             │
             │ (Player B plays here)
             │
    ─────────●─────────→ Virtual Disc Rotation
             │           (speed controls rotation)
             │
             │ (Player A plays here)
             │
             ↓
        Low Swing Zone
```

**Low Swing (swingl):**
- Plays when virtual position is in "Low zone"
- Has one sonic character
- Not inherently "slower"

**High Swing (swingh):**
- Plays when virtual position is in "High zone"
- Has different sonic character
- Not inherently "faster"

**Both files:**
- Play at normal speed (44.1 kHz)
- Loop continuously
- Have equal importance
- Crossfade based on position, NOT speed

### Volume Examples at Different Speeds

#### Slow Swing (100 deg/s), Position in Low Zone:

```
Hum:     Volume 80%  ████████
SwingL:  Volume 40%  ████
SwingH:  Volume 0%
         └─ In Low zone, so Low playing
```

#### Fast Swing (400 deg/s), Position in Low Zone:

```
Hum:     Volume 25%  ██
SwingL:  Volume 100% ██████████
SwingH:  Volume 0%
         └─ Still in Low zone, just louder
```

#### Fast Swing (400 deg/s), Position in High Zone:

```
Hum:     Volume 25%  ██
SwingL:  Volume 0%
SwingH:  Volume 100% ██████████
         └─ Rotated to High zone
```

### How Position Changes with Speed

**Slow Swing (100 deg/s):**
```
Time:  0s────1s────2s────3s────4s────
Pos:   0° → 100° → 200° → 300° → 400°
Zone:  Low───Low────High───High──Low─
```
- Takes 1.8 seconds to go from Low → High (180°)
- You hear Low swing for extended time
- Gradual transition to High

**Fast Swing (400 deg/s):**
```
Time:  0s─────1s─────2s─────
Pos:   0° → 400° → 800°
Zone:  Low─High─Low─High
```
- Takes 0.45 seconds to go from Low → High (180°)
- Rapid alternation between Low and High
- Creates dynamic, changing sound

### Key Insight

The **SAME pair** of files sounds different at different speeds because:

1. **Volume changes** (louder at higher speeds)
2. **Transition frequency** changes (more transitions per second)
3. **Time in each zone** changes (longer in zones at slow speeds)

But the **pitch/Hz of the audio files never changes**.

### Phase 3: Motion Processing

**Main Processing Loop (called every gyro sample):**

```pseudocode
FUNCTION ProcessMotion(gyro_data, delta_time_us):
    // Filter gyro data
    gyro = gyro_filter.filter(gyro_data)

    // Calculate swing speed (Y and Z axes only)
    swing_speed = SQRT(gyro.y² + gyro.z²)

    // Convert delta time to seconds
    delta_t = delta_time_us / 1000000.0

    // State machine
    SWITCH state:
        CASE OFF:
            HandleOffState(swing_speed, delta_t)
            // Fall through to ON if just transitioned
            IF state == ON:
                HandleOnState(swing_speed, delta_t)
            END IF

        CASE ON:
            HandleOnState(swing_speed, delta_t)

        CASE OUT:
            HandleOutState()
    END SWITCH
END FUNCTION
```

### Phase 4: OFF State Logic

**Waiting for Motion:**

```pseudocode
FUNCTION HandleOffState(swing_speed, delta_t):
    // Check threshold
    IF swing_speed > config.SwingStrengthThreshold:
        state = ON
        // Continue to ON state processing
    ELSE:
        // Keep players silent
        PlayerA.set_volume(0.0)
        PlayerB.set_volume(0.0)
        RETURN
    END IF
END FUNCTION
```

### Phase 5: ON State Logic

**Active Swinging:**

```pseudocode
FUNCTION HandleOnState(swing_speed, delta_t):
    // 1. Calculate swing strength (0.0 to 1.0)
    swing_strength = MIN(1.0, swing_speed / config.SwingSensitivity)

    // 2. Check for accent swings (optional)
    IF config.AccentSwingSpeedThreshold > 0:
        IF swing_speed > config.AccentSwingSpeedThreshold:
            swing_accel = calculate_swing_acceleration()

            IF swing_accel > config.AccentSlashAccelerationThreshold:
                trigger_accent_slash()
            ELSE:
                trigger_accent_swing()
            END IF
        END IF
    END IF

    // 3. Check for end of swing (hysteresis)
    threshold_with_hysteresis = config.SwingStrengthThreshold * 0.9
    IF swing_speed < threshold_with_hysteresis:
        state = OUT
        RETURN
    END IF

    // 4. Rotate virtual wheel based on speed
    rotation_amount = -swing_speed * delta_t  // Negative for direction
    PlayerA.rotate(rotation_amount)
    PlayerB.rotate(rotation_amount)

    // 5. Check for transition completion
    WHILE PlayerA.end() < 0.0:
        // Player A has completed its transition zone
        // Position Player B for next transition
        PlayerB.midpoint = PlayerA.midpoint + PlayerA.separation

        // Swap players
        SWAP(PlayerA, PlayerB)
    END WHILE

    // 6. Calculate crossfade mix (A to B)
    mixab = CLAMP(-PlayerA.begin() / PlayerA.width, 0.0, 1.0)

    // mixab = 0.0: Fully on A, before transition
    // mixab = 0.5: Mid-transition
    // mixab = 1.0: Fully on B, after transition

    // 7. Apply power curve to swing strength
    mixhum = POW(swing_strength, config.SwingSharpness)

    // 8. Calculate volumes
    swing_volume = mixhum * config.MaxSwingVolume
    hum_volume = 1.0 - (mixhum * config.MaximumHumDucking / 100.0)

    PlayerA_volume = swing_volume * mixab
    PlayerB_volume = swing_volume * (1.0 - mixab)

    // 9. Apply volumes to players
    PlayerA.set_volume(PlayerA_volume)
    PlayerB.set_volume(PlayerB_volume)
    hum_player.set_volume(hum_volume)
END FUNCTION
```

### Phase 6: OUT State Logic

**Fading Out:**

```pseudocode
FUNCTION HandleOutState():
    // Wait for both players to reach zero volume
    IF PlayerA.volume == 0.0 AND PlayerB.volume == 0.0:
        // Pick new random swing pair
        PickRandomSwing()

        state = OFF
    END IF

    // Volumes fade naturally via VolumeOverlay fade time
END FUNCTION
```

## How Speed Affects Sound

### What Does NOT Change

❌ **Audio file pitch/frequency (Hz)** - Always plays at original pitch
❌ **Playback speed** - Always 44.1 kHz sample rate
❌ **File duration** - 3-second file always takes 3 seconds to loop
❌ **Audio sample rate** - Never sped up or slowed down

### What DOES Change

✅ **Overall volume** - Faster swing = louder sound
✅ **Virtual rotation speed** - How fast you move through the disc
✅ **Transition frequency** - How often crossfades happen
✅ **Time in each zone** - How long you hear each file
✅ **Hum ducking amount** - How much hum reduces

### The Perception Trick

**Why does it FEEL faster at high speeds?**

1. **Volume increase** creates intensity perception
2. **Rapid crossfading** creates movement perception
3. **High files** already have brighter/aggressive character
4. **Hum reduction** emphasizes swing sounds

**It's like a DJ crossfading between two turntables - the music doesn't speed up, but the mixing gets faster!**

### Detailed Comparison

#### Slow Swing (100 deg/s)

```
Timeline (4 seconds):
│████████████Low██████░░░░░░High░░░░│
0s                                4s

Virtual Disc:
- Rotates 400° total (100°/s × 4s)
- Completes 1.1 full rotations
- Crosses Low→High transition 2 times
- Spends ~1.8s in each zone

Volume Levels:
- Swing: 22% of maximum (100/450 = 0.22)
- Hum: 83% (only slightly ducked)

Perception:
✓ Sustained, smooth sound
✓ Gradual transitions
✓ Hum remains prominent
✓ "Gentle" character
```

#### Fast Swing (400 deg/s)

```
Timeline (4 seconds):
│█░█░█░█░█░█░█░█░█░█│
0s                 4s
└─ Many rapid L/H transitions

Virtual Disc:
- Rotates 1600° total (400°/s × 4s)
- Completes 4.4 full rotations
- Crosses Low→High transition 8 times
- Spends ~0.45s in each zone

Volume Levels:
- Swing: 89% of maximum (400/450 = 0.89)
- Hum: 33% (heavily ducked)

Perception:
✓ Dynamic, energetic sound
✓ Rapid transitions
✓ Hum barely audible
✓ "Aggressive" character
```

### Visual Timeline Comparison

```
Slow Swing (100 deg/s):
Time:    0s────1s────2s────3s────4s────
Hum:     ████ ████  ████  ████  ████
SwingL:  ███████████░░░░░░░░░░░░
SwingH:  ░░░░░░░░░░░░███████████
         └─ Gradual ─┘└─ change ─┘

Fast Swing (400 deg/s):
Time:    0s──1s──2s──3s──4s──
Hum:     ██  ██  ██  ██  ██
SwingL:  ██░░██░░██░░██░░
SwingH:  ░░██░░██░░██░░██
         └─┘ └─┘ └─┘ └─┘
         Rapid alternation
```

### The "Two Radios" Analogy

Imagine you have **two radios** (Low and High) playing different songs:

**Both radios are always ON, playing at normal speed**

**Slow Movement:**
```
You:  [Slowly turning dial]
Radio L: ████████░░░░ (hear for 2 seconds)
Radio H: ░░░░████████ (hear for 2 seconds)
Effect: Smooth transition between songs
```

**Fast Movement:**
```
You:  [Rapidly flicking dial back and forth]
Radio L: ██░░██░░██░░
Radio H: ░░██░░██░░██
Effect: Rapidly alternating between songs
```

**The music doesn't speed up - you're just changing stations faster!**

### Rotation Speed Example

```
Position: 0°───────90°────────180°───────270°────────360°
          │        │          │         │          │
Slow:     0s───────0.9s───────1.8s──────2.7s──────3.6s
          └────────Smooth rotation────────────────┘

Fast:     0s───0.225s───0.45s───0.675s───0.9s
          └─────Rapid rotation─────┘
```

At 180° you cross from Low zone to High zone.

**Slow:** Takes 1.8s to cross, long smooth transition
**Fast:** Takes 0.45s to cross, rapid transition

### Why Not Change Playback Speed?

**Could we just speed up/slow down the audio file?**

❌ **Problems:**
1. **Pitch changes** (chipmunk effect at high speed)
2. **Unnatural sound** (doesn't match real saber physics)
3. **CPU intensive** (real-time pitch shifting)
4. **Artifacts** (digital warbling)

✅ **SmoothSwing approach:**
1. **Natural pitch** (files play normally)
2. **Realistic** (mimics blade motion through air)
3. **Efficient** (just volume/crossfade)
4. **Clean** (no processing artifacts)

### Mathematical Proof

**File: swingl01.wav (3 seconds duration)**

At 100 deg/s swing:
- Virtual rotation: 300° in 3 seconds
- File plays: 3 seconds (normal speed)
- Playback rate: 44100 Hz (unchanged)

At 400 deg/s swing:
- Virtual rotation: 1200° in 3 seconds
- File plays: 3 seconds (normal speed)
- Playback rate: 44100 Hz (unchanged)

**Same file duration, different rotation speeds = perception of faster sound**

### Summary Table

| Aspect | Slow (100°/s) | Fast (400°/s) | Changes? |
|--------|---------------|---------------|----------|
| File pitch | Original | Original | ❌ No |
| Playback rate | 44.1 kHz | 44.1 kHz | ❌ No |
| Swing volume | 22% | 89% | ✅ Yes |
| Hum volume | 83% | 33% | ✅ Yes |
| Transitions/sec | 0.5 | 2.0 | ✅ Yes |
| Time per zone | 1.8s | 0.45s | ✅ Yes |
| Perception | Gentle | Aggressive | ✅ Yes |

**Result:** Dynamic, responsive sound without pitch artifacts!

## Mathematical Formulas

### Swing Speed Calculation

```
swing_speed = √(gyro_y² + gyro_z²)
```

Where:
- `gyro_y` = gyroscope Y-axis (degrees/second)
- `gyro_z` = gyroscope Z-axis (degrees/second)

**Rationale:** Only lateral swing matters, not twist (X-axis)

### Swing Strength Calculation

```
swing_strength = min(1.0, swing_speed / SwingSensitivity)
```

**Range:** 0.0 (no swing) to 1.0 (max swing)

**Example:**
- At 0 deg/s: strength = 0.0
- At 225 deg/s: strength = 0.5
- At 450 deg/s: strength = 1.0
- At 900 deg/s: strength = 1.0 (clamped)

### Power Curve Application

```
mixhum = swing_strength ^ SwingSharpness
```

Where `SwingSharpness` = 1.75 (default)

**Effect:** Creates non-linear response
- Low speeds: Gradual volume increase
- High speeds: Rapid volume increase

**Example (SwingSharpness = 1.75):**

| swing_strength | mixhum |
|----------------|--------|
| 0.0 | 0.000 |
| 0.1 | 0.018 |
| 0.2 | 0.053 |
| 0.3 | 0.104 |
| 0.4 | 0.169 |
| 0.5 | 0.250 |
| 0.6 | 0.344 |
| 0.7 | 0.451 |
| 0.8 | 0.573 |
| 0.9 | 0.707 |
| 1.0 | 1.000 |

### Virtual Rotation

```
new_position = old_position - (swing_speed × delta_time)
```

**Negative sign:** Maintains directional consistency

**Position wrap:**
```
while position > 180:
    position -= 360
while position < -180:
    position += 360
```

### Crossfade Calculation

```
mixab = clamp(-PlayerA.begin() / PlayerA.width, 0.0, 1.0)
```

Where:
```
PlayerA.begin() = PlayerA.midpoint - PlayerA.width / 2
```

**Explanation:**
- When `begin()` = 0: In center of transition (mixab = 0.0)
- When `begin()` = -width: At end of transition (mixab = 1.0)
- When `begin()` > 0: Before transition (mixab = 0.0, clamped)

**Graph:**
```
Position:   [-width/2]  [0]  [+width/2]
              Start    Center    End
mixab:        0.0  →   0.5  →   1.0
Player A:     100% →   50%  →    0%
Player B:      0% →   50%  →   100%
```

### Volume Calculations

```
swing_volume = mixhum × MaxSwingVolume
hum_volume = 1.0 - (mixhum × MaximumHumDucking / 100)

PlayerA_volume = swing_volume × mixab
PlayerB_volume = swing_volume × (1.0 - mixab)
```

**Example (MaxSwingVolume = 3.0, MaximumHumDucking = 75):**

| mixhum | swing_vol | hum_vol | Notes |
|--------|-----------|---------|-------|
| 0.0 | 0.0 | 1.00 | No swing, full hum |
| 0.25 | 0.75 | 0.81 | Light swing |
| 0.5 | 1.5 | 0.63 | Medium swing |
| 0.75 | 2.25 | 0.44 | Strong swing |
| 1.0 | 3.0 | 0.25 | Max swing |

## Visual Algorithm Representation

### Virtual Wheel Concept

```
        High Swing (B)
             ↑
             │
             │ Transition Zone (width)
             │
    ─────────┼─────────→ Rotation
             │ (driven by gyro)
             │
        Low Swing (A)
             ↓
```

**Angular Positions:**
- Player A midpoint: 0°
- Player A transition: -22.5° to +22.5° (if width = 45°)
- Separation: 180°
- Player B midpoint: 180°
- Player B transition: 157.5° to 202.5° (if width = 45°)

### Crossfade Diagram

```
Volume
  ^
  │
1 │     ┌────────┐           Player B
  │    ╱          ╲
  │   ╱            ╲
0.5│  ×              ╲        Crossfade point
  │ ╱                ╲
  │╱                  ╲
0 └──────────────────────────────→ Position
      A.begin()    A.end()
     (transition zone)

  Player A: ╲╲╲╲╲╲╲╲
  Player B:         ╱╱╱╱╱╱╱╱
```

### State Flow Diagram

```
┌─────────┐
│  INIT   │
└────┬────┘
     │ Activate()
     ▼
┌─────────┐
│   OFF   │◄────────────┐
│ (idle)  │             │
└────┬────┘             │
     │ speed > 20       │
     ▼                  │
┌─────────┐             │
│   ON    │             │
│(swinging)│            │
└────┬────┘             │
     │ speed < 18       │
     ▼                  │
┌─────────┐             │
│   OUT   │             │
│ (fade)  │             │
└────┬────┘             │
     │ volume = 0       │
     └──────────────────┘
```

## Virtual Disc Model - Deep Dive

### The Core Metaphor

Think of the swing system as a **virtual spinning disc** divided into zones. Your lightsaber's motion controls:
1. **How fast the disc spins** (rotation speed)
2. **Which zone you're in** (determines which sound plays)
3. **How loud the sound is** (volume)

```
              0° (North)
                 │
                 │ High Zone
                 │ (Player B)
        270° ────●──── 90°
         (West)  │  (East)
                 │ Low Zone
                 │ (Player A)
                 │
              180° (South)
```

### Detailed Zone Map

```
     ┌──────────────────────────────┐
     │       High Swing Zone        │
     │    (Player B: swingh)        │
     │                              │
  ───┼─── Transition 2 ────────────┼───  202.5°
     │    (width: 160°)             │
     │   ░░░░░░░░░░░░░░░░░░         │
180°─┼─ B midpoint ─────────────────┼─  B plays here
     │   ░░░░░░░░░░░░░░░░░░         │
     │    Crossfade zone            │
  ───┼──────────────────────────────┼───  157.5°
     │                              │
     │     (silence region)         │
     │                              │
  ───┼──────────────────────────────┼───  22.5°
     │    Transition 1              │
     │    (width: 45°)              │
     │   ▓▓▓▓▓▓▓▓▓▓▓▓               │
  0°─┼─ A midpoint ───────────── ──┼─  A plays here
     │   ▓▓▓▓▓▓▓▓▓▓▓▓               │
     │    Crossfade zone            │
  ───┼──────────────────────────────┼───  -22.5°
     │                              │
     │       Low Swing Zone         │
     │    (Player A: swingl)        │
     └──────────────────────────────┘
```

### Position Tracking Example

**Starting Position: 0° (in Low Zone)**

```
Gyro reads: Y=100°/s, Z=0°/s
Swing speed = √(100² + 0²) = 100°/s
Delta time = 0.1s

New position = 0° - (100° × 0.1s) = -10°
               └─ Negative because rotation direction

After wrapping: -10° (still in Low Zone)
```

**Continuous Motion:**
```
Time  | Speed   | Rotation | Position | Zone   | Volume A | Volume B
------|---------|----------|----------|--------|----------|----------
0.0s  | 100°/s  | -10°     | -10°     | Low    | 50%      | 0%
0.1s  | 150°/s  | -15°     | -25°     | Low    | 75%      | 0%
0.2s  | 200°/s  | -20°     | -45°     | Low    | 90%      | 0%
0.3s  | 250°/s  | -25°     | -70°     | Low    | 100%     | 0%
0.4s  | 300°/s  | -30°     | -100°    | Low    | 100%     | 0%
0.5s  | 350°/s  | -35°     | -135°    | Low    | 100%     | 0%
0.6s  | 400°/s  | -40°     | -175°    | Trans  | 50%      | 50%
0.7s  | 400°/s  | -40°     | 145°     | High   | 0%       | 100%
      | (wrapped)         | (+360°)  |        |          |
```

### Rotation Speed Visualization

**Slow Rotation (100°/s):**
```
Time Scale: 1 second per line

0s:  Position 0° ●
     └─ Low Zone (A playing)

1s:  Position -100° ●
     └─ Still Low Zone

2s:  Position -200° = 160° ●
     └─ High Zone (B playing)

3s:  Position 60° ●
     └─ Low Zone (A playing again)

Characteristics:
- Smooth transitions
- Long time in each zone
- Gradual volume changes
```

**Fast Rotation (400°/s):**
```
Time Scale: 1 second per line

0s:   Position 0° ●
      └─ Low Zone

0.25s: Position -100° ●
      └─ Low Zone

0.5s:  Position -200° = 160° ●
      └─ High Zone (crossover!)

0.75s: Position -100° ●
      └─ Low Zone (crossover again!)

1s:   Position 0° ●
      └─ Back to start (full rotation!)

Characteristics:
- Rapid transitions
- Quick zone changes
- Dynamic volume shifts
```

### Crossfade Mechanics

**Entering Transition Zone:**

```
Position approaching -22.5° (entering A's transition):

Position: -22.5° ────────── 0° ────────── +22.5°
          │ Start           │ Center      │ End
          │                 │             │
Volume A: 100% ──────────── 50% ────────── 0%
          ████████████      ██████
                  ╲          ╲
                   ╲          ╲
Volume B: 0%  ──────────────  50% ────────── 100%
                             ██████        ██████████

Crossfade:        Linear interpolation
                  mixab = -A.begin() / A.width
```

**Calculation Example:**
```
A.midpoint = 0°
A.width = 45°
A.begin() = 0° - 45°/2 = -22.5°

Current position = -10°

mixab = -(-22.5° - (-10°)) / 45°
      = -(-12.5°) / 45°
      = 12.5° / 45°
      = 0.278

Volume A = swing_volume × (1 - 0.278) = swing_volume × 0.722
Volume B = swing_volume × 0.278
```

### The "Two Radios" Analogy Explained

```
┌─────────────────────────────────────────┐
│          Radio A (Low Swing)            │
│  Station: 99.1 FM                       │
│  Playing: swingl02.wav                  │
│  Status: ON (always transmitting)       │
│  Your dial position: [●············]    │
│  What you hear: ████████ (loud)         │
└─────────────────────────────────────────┘

┌─────────────────────────────────────────┐
│          Radio B (High Swing)           │
│  Station: 102.5 FM                      │
│  Playing: swingh02.wav                  │
│  Status: ON (always transmitting)       │
│  Your dial position: [············●]    │
│  What you hear: ░░░░ (quiet)            │
└─────────────────────────────────────────┘

┌─────────────────────────────────────────┐
│          Your Control                   │
│  Swing speed → How fast you turn dial   │
│  Position → Which station louder        │
│  Crossfade → Gradual dial turning       │
└─────────────────────────────────────────┘
```

**When you swing slowly:**
- You turn the dial gradually
- Smooth transition A → B → A
- Like a gentle radio scan

**When you swing fast:**
- You flick the dial rapidly back and forth
- Quick alternation A ↔ B ↔ A ↔ B
- Like channel surfing

**Key insight:** The music on each station doesn't speed up - you just change stations faster!

### Complete Rotation Cycle

```
Full 360° rotation at 200°/s (1.8 seconds):

Start: 0°
│
├─ Low Zone (A dominant)
│  Hear: swingl playing
│  Duration: ~0.9s
│
├─ 180° (Transition point)
│  Hear: Crossfade A→B
│  Duration: ~0.45s (width ÷ speed)
│
├─ High Zone (B dominant)
│  Hear: swingh playing
│  Duration: ~0.9s
│
├─ 360° = 0° (Transition point)
│  Hear: Crossfade B→A
│  Duration: ~0.45s
│
End: Back to start (cycle repeats)
```

### Multi-Speed Comparison

```
               Slow (100°/s)  Medium (250°/s)  Fast (400°/s)
              ┌──────────────┬───────────────┬──────────────┐
Time for      │              │               │              │
full rotation │    3.6s      │    1.44s      │    0.9s      │
              ├──────────────┼───────────────┼──────────────┤
Time in       │              │               │              │
each zone     │    1.8s      │    0.72s      │    0.45s     │
              ├──────────────┼───────────────┼──────────────┤
Transitions   │              │               │              │
per second    │    0.56      │    1.39       │    2.22      │
              ├──────────────┼───────────────┼──────────────┤
Perception    │   Smooth,    │   Moderate    │   Dynamic,   │
              │   sustained  │   motion      │   aggressive │
              └──────────────┴───────────────┴──────────────┘
```

### Why This Model Works

**Advantages:**

1. **Natural Feel:**
   - Mimics blade cutting through air
   - Faster motion = more "wind" sound

2. **No Artifacts:**
   - No pitch shifting needed
   - No digital processing
   - Clean, pure audio

3. **Efficient:**
   - Simple volume control
   - Linear crossfading
   - Low CPU usage

4. **Variation:**
   - Random pair selection
   - Random starting positions
   - Never sounds repetitive

5. **Responsive:**
   - Instant reaction to motion
   - Smooth transitions
   - No latency

### Common Questions

**Q: Why not just speed up/slow down the audio?**
A: Would cause pitch changes (chipmunk effect) and require expensive real-time processing.

**Q: Why use two files (Low/High) instead of one?**
A: Creates directional variety and prevents monotonous looping.

**Q: Why does faster motion sound more intense?**
A: Combination of louder volume + rapid transitions + hum ducking creates intensity perception.

**Q: How many files are playing at once?**
A: Always 3: Hum + SwingL + SwingH (but Swing volumes vary based on position).

**Q: Does the virtual position reset between swings?**
A: No, it continues from where it stopped, adding natural variation.

## Implementation Example (Pseudocode)

### Complete Algorithm

```cpp
class SmoothSwingV2 {
private:
    struct SwingData {
        BufferedWavPlayer* player;
        float midpoint;
        float width;
        float separation;

        float begin() { return midpoint - width / 2.0; }
        float end() { return midpoint + width / 2.0; }

        void rotate(float deg) {
            midpoint += deg;
            while (midpoint > 180.0) midpoint -= 360.0;
            while (midpoint < -180.0) midpoint += 360.0;
        }

        void set_volume(float vol) {
            int volume_14bit = vol * 16384;
            player->set_volume(volume_14bit);
        }
    };

    SwingData A, B;
    enum { OFF, ON, OUT } state;
    BoxFilter<Vec3, 3> gyro_filter;
    SmoothSwingConfig config;
    uint32_t last_time;

public:
    void Activate() {
        // Load sound files (implementation specific)
        load_swing_files();

        A.separation = config.Low2HighSeparationDegrees;
        B.separation = config.High2LowSeparationDegrees;

        PickRandomSwing();
        state = OFF;
    }

    void PickRandomSwing() {
        int pair = random(num_swing_pairs);
        A.player->play(swingl_files[pair]);
        B.player->play(swingh_files[pair]);

        float offset = random(10.0, 60.0);
        A.midpoint = offset;
        A.width = config.Transition1Degrees;

        B.midpoint = A.midpoint + A.separation;
        B.width = config.Transition2Degrees;

        if (random(2) == 1) {
            swap(A, B);
        }

        A.set_volume(0.0);
        B.set_volume(0.0);
    }

    void ProcessMotion(Vec3 gyro_raw, uint32_t timestamp) {
        Vec3 gyro = gyro_filter.filter(gyro_raw);
        float speed = sqrt(gyro.y * gyro.y + gyro.z * gyro.z);

        float delta_t = (timestamp - last_time) / 1000000.0;
        last_time = timestamp;

        switch (state) {
            case OFF:
                if (speed > config.SwingStrengthThreshold) {
                    state = ON;
                    // Fall through
                } else {
                    A.set_volume(0.0);
                    B.set_volume(0.0);
                    break;
                }

            case ON: {
                float strength = min(1.0, speed / config.SwingSensitivity);

                if (speed < config.SwingStrengthThreshold * 0.9) {
                    state = OUT;
                    break;
                }

                A.rotate(-speed * delta_t);
                B.rotate(-speed * delta_t);

                while (A.end() < 0.0) {
                    B.midpoint = A.midpoint + A.separation;
                    swap(A, B);
                }

                float mixab = clamp(-A.begin() / A.width, 0.0, 1.0);
                float mixhum = pow(strength, config.SwingSharpness);

                float swing_vol = mixhum * config.MaxSwingVolume;
                float hum_vol = 1.0 - mixhum * config.MaximumHumDucking / 100.0;

                A.set_volume(swing_vol * mixab);
                B.set_volume(swing_vol * (1.0 - mixab));
                hum_player->set_volume(hum_vol);
                break;
            }

            case OUT:
                if (A.player->volume() == 0 && B.player->volume() == 0) {
                    PickRandomSwing();
                    state = OFF;
                }
                break;
        }
    }
};
```

## Tuning Guidelines

### SwingSensitivity

**Higher value (e.g., 600):**
- Requires faster swings to reach max volume
- More dynamic range
- Better for aggressive fighting styles

**Lower value (e.g., 300):**
- Reaches max volume with slower swings
- More responsive
- Better for smooth, controlled movements

### SwingSharpness

**Higher value (e.g., 2.5):**
- Volume stays low longer
- Sudden jump at high speeds
- More dramatic response

**Lower value (e.g., 1.0):**
- Linear response
- Gradual volume increase
- More predictable

### MaximumHumDucking

**Higher value (e.g., 90%):**
- Hum almost silent during swings
- Emphasizes swing sounds
- More dramatic contrast

**Lower value (e.g., 50%):**
- Hum remains audible
- Subtler effect
- More continuous soundscape

### Transition Widths

**Wider transitions (e.g., 90° / 200°):**
- Smoother crossfades
- Less abrupt changes
- Better for flowing movements

**Narrower transitions (e.g., 30° / 120°):**
- Sharper changes
- More distinct low/high separation
- Better for angular movements

## Common Issues & Solutions

### Issue: Swing sounds don't trigger

**Solution:** Lower `SwingStrengthThreshold` (try 15 or 10)

### Issue: Swing sounds too loud

**Solution:** Reduce `MaxSwingVolume` (try 2.0 or 1.5)

### Issue: Hum disappears too much

**Solution:** Reduce `MaximumHumDucking` (try 50%)

### Issue: Abrupt transitions

**Solution:** Increase `Transition1Degrees` and `Transition2Degrees`

### Issue: Not responsive enough

**Solution:**
- Reduce `SwingSensitivity` (try 300)
- Reduce `SwingSharpness` (try 1.0)

## Performance Characteristics

### CPU Usage (per call)
- Filtering: ~10 μs
- Calculations: ~20 μs
- Volume updates: ~30 μs
- **Total:** ~60 μs @ 240 MHz

### Memory Usage
- State data: ~100 bytes
- Filters: ~50 bytes
- **Total:** ~150 bytes

### Call Frequency
- Ideal: Every gyro sample (800-1600 Hz)
- Minimum: 400 Hz
- Typical: 800 Hz

## Next Steps

For complete implementation:
- **Motion Integration:** See `motion-processing.md`
- **Audio Mixing:** See `audio-mixing-pipeline.md`
- **Sound Files:** See `sound-font-structure.md`
