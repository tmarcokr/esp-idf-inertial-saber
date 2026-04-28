# ProffieOS Effects Catalog - Technical Reference

**Based on:** ProffieOS Source Code Analysis

**Last Updated:** 2026-04-06

**Purpose:** Complete technical reference for motion detection algorithms, effect triggers, and configuration parameters with exact source code references.

---

## Table of Contents

1. [Motion Detection System](#motion-detection-system)
2. [Motion-Based Effects](#motion-based-effects)
3. [Lockup System](#lockup-system)
4. [Configuration Parameters](#configuration-parameters)
5. [Implementation Complexity](#implementation-complexity)

---

## Motion Detection System

### Fusor Class (common/fuse.h)

The `Fusor` class is the core motion processing engine that fuses accelerometer and gyroscope data to compute all motion metrics used throughout ProffieOS.

#### Sampling Rates

**Source:** common/fuse.h:12-17

```cpp
#define ACCEL_MEASUREMENTS_PER_SECOND 800
#define GYRO_MEASUREMENTS_PER_SECOND 800
```

Both sensors sample at **800 Hz** by default.

#### Motion Metrics

All metrics are computed in the `Loop()` method (fuse.h:100-225):

| Metric | Method | Units | Description | Line Reference |
|--------|--------|-------|-------------|----------------|
| `accel_` | `accel()` | G | Current acceleration vector (3-axis) | fuse.h:121-122, 320 |
| `gyro_` | `gyro()` | degrees/s | Current angular velocity (3-axis) | fuse.h:123-124, 315 |
| `down_` | `down()` | G | Gravity vector (fused estimate) | fuse.h:88, 136, 322 |
| `mss_` | `mss()` | m/s² | Acceleration minus gravity | fuse.h:164-165, 321 |
| `swing_speed_` | `swing_speed()` | degrees/s | Magnitude of swing rotation | fuse.h:257-266 |
| `angle1_` | `angle1()` | radians | Blade tip angle (up/down) | fuse.h:233-239 |
| `angle2_` | `angle2()` | radians | Twist angle | fuse.h:244-249 |

#### Key Formulas

**Gravity Vector Fusion** (fuse.h:148-196):

The system uses a complementary filter to fuse gyroscope and accelerometer data:

```cpp
// Weight factor calculation (line 148-162)
float wGyro = 1.0;
wGyro += gyro_.len() / 100.0;              // Trust accel less when spinning
wGyro += fabsf(accel_.len() - 1.0f) * 50.0; // Trust accel less when not 1G
wGyro += accel_extrapolator_.slope().len() * 1000; // Trust accel less when changing

// Fusion (line 189-191)
float gyro_factor = expf(logf(0.01) * delta_t / wGyro);
down_ = down_ * gyro_factor + accel_ * (1.0 - gyro_factor);
```

**Swing Speed Calculation** (fuse.h:257-266):

```cpp
swing_speed_ = sqrtf(gyro_.z * gyro_.z + gyro_.y * gyro_.y);
```

This uses only Y and Z gyroscope axes (perpendicular to blade axis).

**Blade Angle Calculation** (fuse.h:233-239):

```cpp
// PI/2 = straight up, -PI/2 = straight down
angle1_ = atan2f(down_.x, sqrtf(down_.z * down_.z + down_.y * down_.y));
```

**Twist Angle** (fuse.h:244-249):

```cpp
// Range: -PI to PI
angle2_ = atan2f(down_.y, down_.z);
```

#### Clash Detection Metrics

**Clash Acceleration Filter** (fuse.h:325-327):

```cpp
Vec3 clash_mss() {
  return accel_clash_filter_.get() - down_;
}
```

Uses a box filter at 1600 Hz (clash_filter_hz = 1600, line 391) to detect sudden impacts.

**Gyro Clash Value** (fuse.h:330-344):

```cpp
float gyro_clash_value() {
  float v = (gyro_clash_filter_.get() - gyro_extrapolator_.get(micros())).len();
  return v / 9.81;  // Translate to m/s² assuming 1 meter blade
}
```

Detects sudden changes in rotation rate.

#### Motion Derivatives

**Gyro Slope** (fuse.h:316-319):

```cpp
Vec3 gyro_slope() {
  return gyro_extrapolator_.slope() * 1000000;  // degrees/s²
}
```

**Swing Acceleration** (fuse.h:351-354):

```cpp
float swing_accel() {
  Vec3 gyro_slope_ = gyro_slope();
  return sqrtf(gyro_slope_.z * gyro_slope_.z + gyro_slope_.y * gyro_slope_.y) * (M_PI / 180);
}
```

Returns radians/s².

**Twist Acceleration** (fuse.h:357-359):

```cpp
float twist_accel() {
  return gyro_slope().x;
}
```

Returns degrees/s².

#### Stabilization

**Stabilization Time** (fuse.h:96-98):

```cpp
#define GYRO_STABILIZATION_TIME_MS 64
```

Data is ignored for 64ms after a clear/reset to allow sensors to stabilize (fuse.h:104-113).

---

## Motion-Based Effects

### 1. Clash Detection

**Source:** props/prop_base.h:802-851

**Algorithm:**

The clash detection combines accelerometer and gyroscope data with dynamic thresholds.

**Core Detection Logic** (prop_base.h:805-851):

```cpp
void DoAccel(const Vec3& accel, bool clear) {
  fusor.DoAccel(accel, clear);
  accel_loop_counter_.Update();
  Vec3 diff = fusor.clash_mss();
  float v;

  if (clear) {
    accel_ = accel;
    diff = Vec3(0,0,0);
    v = 0.0;
  } else {
#ifndef PROFFIEOS_DONT_USE_GYRO_FOR_CLASH
    v = (diff.len() + fusor.gyro_clash_value()) / 2.0;  // Line 813
#else
    v = diff.len();
#endif
  }

  // Dynamic threshold calculation (lines 830-835)
  if (v > (CLASH_THRESHOLD_G * (1
                                + fusor.gyro().len() / 500.0
#if defined(ENABLE_AUDIO) && defined(AUDIO_CLASH_SUPPRESSION_LEVEL)
                                + dynamic_mixer.audio_volume() * (AUDIO_CLASH_SUPPRESSION_LEVEL * 1E-10) * dynamic_mixer.get_volume()
#endif
                                ))) {
    // Clash detected!
  }
}
```

**Threshold Formula** (prop_base.h:830-835):

```
effective_threshold = CLASH_THRESHOLD_G × (1 + gyro_adjustment + audio_adjustment)

where:
  gyro_adjustment = gyro().len() / 500.0
  audio_adjustment = dynamic_mixer.audio_volume() × (AUDIO_CLASH_SUPPRESSION_LEVEL × 1E-10) × volume
```

**Configuration Parameters:**

| Parameter | Default | Line | Description |
|-----------|---------|------|-------------|
| `CLASH_THRESHOLD_G` | (user-defined) | prop_base.h:196 | Base clash threshold in G |
| `AUDIO_CLASH_SUPPRESSION_LEVEL` | 10 | prop_base.h:25 | Audio volume impact (range 1-50) |

**Clash vs Stab Discrimination** (prop_base.h:839-840):

```cpp
bool stab = diff.x < -2.0 * sqrtf(diff.y * diff.y + diff.z * diff.z) &&
            fusor.swing_speed() < 150;
```

A motion is classified as a stab if:
- Forward acceleration (X-axis) is strongly negative (impact at tip)
- Lateral acceleration is less than half of forward
- Swing speed is below 150 degrees/s

**Implementation Complexity:** ⭐⭐⭐
- Moderate: Real-time filtering, dynamic thresholds
- Dependencies: Fusor, audio mixer
- CPU: Runs in interrupt context, must be fast

---

### 2. Stab Detection

**Source:** props/prop_base.h:839-840

**Algorithm:**

Stab is detected as part of the clash detection logic with additional constraints.

**Detection Conditions:**

```cpp
diff.x < -2.0 * sqrtf(diff.y * diff.y + diff.z * diff.z)  // Forward impact dominant
fusor.swing_speed() < 150                                  // Not swinging
```

**Physical Interpretation:**
- Thrust motion with impact at blade tip
- Forward deceleration exceeds lateral by 2x
- Minimal rotation during impact

**Thresholds:**

| Threshold | Value | Description |
|-----------|-------|-------------|
| Forward/Lateral ratio | 2.0 | Forward accel must be 2x lateral |
| Max swing speed | 150 deg/s | Distinguishes from clash during swing |

**Implementation Complexity:** ⭐⭐
- Simple: Piggybacks on clash detection
- Dependencies: Clash detection system
- CPU: No additional overhead

---

### 3. Twist Detection

**Source:** props/prop_base.h:968-1008

**Algorithm:**

Twist detection uses a stroke-based gesture recognizer that identifies alternating left/right rotation strokes.

**Stroke Detection** (prop_base.h:975-984):

```cpp
bool DetectTwistStrokes() {
  Vec3 gyro = fusor.gyro();
  if (fabsf(gyro.x) > 200.0 &&
      fabsf(gyro.x) > 3.0f * abs(gyro.y) &&
      fabsf(gyro.x) > 3.0f * abs(gyro.z)) {
    return DoGesture(gyro.x > 0 ? TWIST_RIGHT : TWIST_LEFT);
  } else {
    return DoGesture(TWIST_CLOSE);
  }
}
```

**Twist Recognition** (prop_base.h:987-1007):

```cpp
bool ProcessTwistEvents() {
  if ((strokes[NELEM(strokes)-1].type == TWIST_LEFT &&
       strokes[NELEM(strokes)-2].type == TWIST_RIGHT) ||
      (strokes[NELEM(strokes)-1].type == TWIST_RIGHT &&
       strokes[NELEM(strokes)-2].type == TWIST_LEFT)) {
    // Check stroke lengths (90-300ms each)
    if (strokes[NELEM(strokes)-1].length() > 90UL &&
        strokes[NELEM(strokes)-1].length() < 300UL &&
        strokes[NELEM(strokes)-2].length() > 90UL &&
        strokes[NELEM(strokes)-2].length() < 300UL) {
      // Check separation (< 200ms between strokes)
      uint32_t separation = strokes[NELEM(strokes)-1].start_millis -
                           strokes[NELEM(strokes)-2].end_millis;
      if (separation < 200UL) {
        Event(BUTTON_NONE, EVENT_TWIST);
        return true;
      }
    }
  }
  return false;
}
```

**Thresholds:**

| Parameter | Value | Description | Line |
|-----------|-------|-------------|------|
| Min rotation rate | 200 deg/s | Minimum twist speed | prop_base.h:977 |
| Axis dominance ratio | 3.0 | X-axis must be 3x stronger than Y/Z | prop_base.h:978-979 |
| Min stroke duration | 90 ms | Minimum twist motion time | prop_base.h:992, 994 |
| Max stroke duration | 300 ms | Maximum twist motion time | prop_base.h:993, 995 |
| Max stroke separation | 200 ms | Time between left/right strokes | prop_base.h:999 |

**Pseudocode:**

```
FUNCTION DetectTwist():
  gyro = current_gyro_reading()

  IF abs(gyro.x) > 200 AND
     abs(gyro.x) > 3 × max(abs(gyro.y), abs(gyro.z)):
    RECORD stroke_direction(gyro.x > 0 ? RIGHT : LEFT)
  ELSE:
    RECORD stroke_ended()

  IF last_two_strokes_are_opposite_directions():
    IF both_strokes_duration IN [90ms, 300ms]:
      IF separation_between_strokes < 200ms:
        TRIGGER twist_event()
```

**Implementation Complexity:** ⭐⭐⭐⭐
- Complex: Gesture state machine, timing windows
- Dependencies: Fusor gyroscope
- CPU: Low overhead, event-driven

---

### 4. Shake Detection

**Source:** props/prop_base.h:1011-1038

**Algorithm:**

Shake detection requires 5 alternating forward/backward thrust strokes in quick succession.

**Stroke Detection** (prop_base.h:1011-1020):

```cpp
void DetectShake() {
  Vec3 mss = fusor.mss();
  bool process = false;
  if (mss.y * mss.y + mss.z * mss.z < 16.0 &&
      (mss.x > 7 || mss.x < -6) &&
      fusor.swing_speed() < 150) {
    process = DoGesture(mss.x > 0 ? SHAKE_FWD : SHAKE_REW);
  } else {
    process = DoGesture(SHAKE_CLOSE);
  }
```

**Shake Validation** (prop_base.h:1021-1036):

```cpp
if (process) {
  int i;
  for (i = 0; i < 5; i++) {
    // Check alternating pattern: FWD, REW, FWD, REW, FWD
    if (strokes[NELEM(strokes)-1-i].type !=
        ((i & 1) ? SHAKE_REW : SHAKE_FWD)) break;
    if (i) {
      uint32_t separation = strokes[NELEM(strokes)-i].start_millis -
                           strokes[NELEM(strokes)-1-i].end_millis;
      if (separation > 250) break;  // Too slow
    }
  }
  if (i == 5) {
    // All 5 strokes valid!
    Event(BUTTON_NONE, EVENT_SHAKE);
  }
}
```

**Thresholds:**

| Parameter | Value | Description | Line |
|-----------|-------|-------------|------|
| Max lateral acceleration | 4 m/s² | Y²+Z² < 16 | prop_base.h:1014 |
| Forward threshold | +7 m/s² | Minimum forward thrust | prop_base.h:1015 |
| Backward threshold | -6 m/s² | Minimum backward thrust | prop_base.h:1015 |
| Max swing speed | 150 deg/s | Must not be swinging | prop_base.h:1016 |
| Number of strokes | 5 | Alternating forward/back | prop_base.h:1023 |
| Max stroke separation | 250 ms | Time between strokes | prop_base.h:1030 |

**Pseudocode:**

```
FUNCTION DetectShake():
  mss = current_linear_acceleration()

  IF lateral_accel² < 16 AND
     (forward_accel > 7 OR forward_accel < -6) AND
     swing_speed < 150:
    RECORD stroke_direction(forward ? FWD : BACK)
  ELSE:
    RECORD stroke_ended()

  IF stroke_just_completed():
    IF last_5_strokes_alternate(FWD, BACK, FWD, BACK, FWD):
      IF all_separations < 250ms:
        TRIGGER shake_event()
```

**Implementation Complexity:** ⭐⭐⭐⭐
- Complex: Multi-stroke pattern matching, tight timing
- Dependencies: Fusor mss()
- CPU: Low, event-driven

---

### 5. Swing Detection

**Source:** props/prop_base.h:1043-1051

**Algorithm:**

Simple hysteresis-based detection using swing speed with different on/off thresholds.

**Code** (prop_base.h:1043-1051):

```cpp
void DetectSwing() {
  if (!swinging_ && fusor.swing_speed() > 250) {
    swinging_ = true;
    Event(BUTTON_NONE, EVENT_SWING);
  }
  if (swinging_ && fusor.swing_speed() < 100) {
    swinging_ = false;
  }
}
```

**Thresholds:**

| Threshold | Value | Description | Line |
|-----------|-------|-------------|------|
| Start threshold | 250 deg/s | Speed to start swing | prop_base.h:1044 |
| Stop threshold | 100 deg/s | Speed to end swing | prop_base.h:1048 |

**Hysteresis:** 150 deg/s gap prevents rapid on/off toggling.

**Pseudocode:**

```
FUNCTION DetectSwing():
  speed = swing_speed()

  IF NOT currently_swinging AND speed > 250:
    currently_swinging = TRUE
    TRIGGER swing_event()

  IF currently_swinging AND speed < 100:
    currently_swinging = FALSE
```

**Implementation Complexity:** ⭐
- Very simple: Two thresholds, single state variable
- Dependencies: Fusor swing_speed()
- CPU: Minimal

---

### 6. Spin Detection

**Source:** sound/hybrid_font.h:338-344, 354-360

**Algorithm:**

Spin detection accumulates rotation angle and triggers when a threshold is exceeded.

**Code** (hybrid_font.h:338-344):

```cpp
#ifdef ENABLE_SPINS
if (angle_ > font_config.ProffieOSSpinDegrees) {
  if (SFX_spin) {
    SaberBase::DoEffect(EFFECT_SPIN, 0);
  }
  angle_ -= font_config.ProffieOSSpinDegrees;
}
#endif
```

**Configuration:**

| Parameter | Default | Description | Line |
|-----------|---------|-------------|------|
| `ProffieOSSpinDegrees` | 360.0 | Rotation to trigger spin | hybrid_font.h:23, 114-116 |

**Requirements:**

- `ENABLE_SPINS` must be defined (compile-time)
- `spin.wav` files must exist in font
- Works with both polyphonic and monophonic fonts

**Accumulation Logic:**

The `angle_` variable accumulates gyroscope rotation over time. When it exceeds `ProffieOSSpinDegrees`, a spin effect is triggered and the angle is decremented by that amount (allowing continuous spin detection).

**Implementation Complexity:** ⭐⭐
- Simple: Angle accumulation with threshold
- Dependencies: Gyroscope integration
- CPU: Very low

---

### 7. Blade Angle

**Source:** functions/blade_angle.h:20-40, common/fuse.h:233-239

**Algorithm:**

Blade angle is computed from the fused gravity vector.

**Angle Calculation** (fuse.h:233-239):

```cpp
// PI/2 = straight up, -PI/2 = straight down
float angle1() {
  if (angle1_ == 1000.0f) {
    angle1_ = atan2f(down_.x, sqrtf(down_.z * down_.z + down_.y * down_.y));
  }
  return angle1_;
}
```

**Function Template** (blade_angle.h:20-40):

```cpp
template<class MIN = Int<0>, class MAX = Int<32768>>
class BladeAngleXSVF {
public:
  int calculate(BladeBase* blade) {
    int min = min_.calculate(blade);
    int max = max_.calculate(blade);
    // v is 0-32768, 0 = straight down, 32768 = straight up
    float v = (fusor.angle1() + M_PI / 2) * 32768 / M_PI;
    return clampi32((v - min) * 32768.0f / (max - min), 0, 32768);
  }
};
```

**Range Mapping:**

| Orientation | angle1() (radians) | Raw Value (0-32768) | Description |
|-------------|-------------------|---------------------|-------------|
| Straight Down | -π/2 | 0 | Tip pointing down |
| Horizontal | 0 | 16384 | Blade parallel to ground |
| Straight Up | +π/2 | 32768 | Tip pointing up |

**Usage in Styles:**

```cpp
BladeAngle<0, 16384>  // Maps horizontal-to-down to 0-32768
BladeAngle<16384, 32768>  // Maps horizontal-to-up to 0-32768
```

**Implementation Complexity:** ⭐⭐
- Simple: Trigonometric calculation from gravity vector
- Dependencies: Fusor down vector
- CPU: Computed once per frame, cached

---

### 8. Force Push (Fett263 Prop)

**Source:** props/saber_fett263_buttons.h:2588-2601

**Algorithm:**

Force push detects a sustained forward thrust motion without rotation.

**Detection Code** (saber_fett263_buttons.h:2588-2601):

```cpp
// EVENT_PUSH
if (!menu_ && fabs(mss.x) < 3.0 &&
    mss.y * mss.y + mss.z * mss.z > 100 &&
    fusor.swing_speed() < 20 &&
    fabs(fusor.gyro().x) < 5) {
  if (saved_gesture_control.forcepush &&
      millis() - push_begin_millis_ > saved_gesture_control.forcepushlen) {
    // Checking for Clash at end of movement
    clash_type_ = CLASH_CHECK;
    push_begin_millis_ = millis();
    clash_impact_millis_ = millis();
  }
} else {
  push_begin_millis_ = millis();
}
```

**Thresholds:**

| Parameter | Value | Description | Line |
|-----------|-------|-------------|------|
| Max forward accel | 3.0 m/s² | abs(mss.x) < 3.0 | saber_fett263_buttons.h:2589 |
| Min lateral accel² | 100 m/s⁴ | Y²+Z² > 100 | saber_fett263_buttons.h:2590 |
| Max swing speed | 20 deg/s | Not swinging | saber_fett263_buttons.h:2591 |
| Max twist rate | 5 deg/s | Not twisting | saber_fett263_buttons.h:2592 |
| `FETT263_FORCE_PUSH_LENGTH` | 5 ms | Duration threshold | saber_fett263_buttons.h:784 |

**Configuration:**

```cpp
#define FETT263_FORCE_PUSH_LENGTH 5  // Range: 1-10
```

**Requirements:**

- `FETT263_FORCE_PUSH` or `FETT263_FORCE_PUSH_ALWAYS_ON` must be defined
- `push.wav` or `force.wav` must exist in font

**Pseudocode:**

```
FUNCTION DetectForcePush():
  mss = current_linear_acceleration()

  IF abs(forward_accel) < 3.0 AND
     lateral_accel² > 100 AND
     swing_speed < 20 AND
     twist_rate < 5:

    IF gesture_sustained_for > FORCE_PUSH_LENGTH:
      TRIGGER force_push_event()
      RESET timer
  ELSE:
    RESET timer
```

**Implementation Complexity:** ⭐⭐⭐
- Moderate: Timer-based sustained gesture
- Dependencies: Fusor mss(), gyro(), swing_speed()
- CPU: Low overhead

---

## Lockup System

**Source:** common/saber_base.h:364-372, 382-385

**Lockup Types Enumeration** (saber_base.h:364-372):

```cpp
enum LockupType {
  LOCKUP_NONE,              // No lockup active
  LOCKUP_NORMAL,            // Blade-to-blade lockup
  LOCKUP_DRAG,              // Dragging blade on ground
  LOCKUP_ARMED,             // For detonators and such
  LOCKUP_AUTOFIRE,          // For blasters and phasers
  LOCKUP_MELT,              // Cutting through doors
  LOCKUP_LIGHTNING_BLOCK,   // Lightning block lockup
};
```

**State Management** (saber_base.h:374-385):

```cpp
static LockupType lockup_;
static BladeSet lockup_blades_;

static LockupType Lockup() { return lockup_; }

static LockupType LockupForBlade(int blade) {
  if (!lockup_blades_[blade]) return LOCKUP_NONE;
  return lockup_;
}

static void SetLockup(LockupType lockup, BladeSet blades = BladeSet::all()) {
  lockup_ = lockup;
  lockup_blades_ = blades;
}
```

**Lockup Descriptions:**

| Type | Value | Use Case | Typical Trigger |
|------|-------|----------|-----------------|
| `LOCKUP_NONE` | 0 | No lockup | N/A |
| `LOCKUP_NORMAL` | 1 | Blade contact | Clash + hold button |
| `LOCKUP_DRAG` | 2 | Blade dragging | Stab down + hold button |
| `LOCKUP_ARMED` | 3 | Armed state | Special mode |
| `LOCKUP_AUTOFIRE` | 4 | Blaster mode | Blaster props |
| `LOCKUP_MELT` | 5 | Melting objects | Stab parallel/up + hold |
| `LOCKUP_LIGHTNING_BLOCK` | 6 | Force lightning | Button combo |

**Blade Set Support:**

Lockups can be applied to specific blades or all blades using the `BladeSet` parameter. This allows multi-blade sabers to have different blades in different lockup states.

**Implementation Complexity:** ⭐⭐
- Simple: Enumeration and state variables
- Dependencies: None
- CPU: Negligible (just state storage)

---

## Configuration Parameters

### Motion Detection Parameters

| Parameter | Default | Range | Units | Source | Description |
|-----------|---------|-------|-------|--------|-------------|
| `ACCEL_MEASUREMENTS_PER_SECOND` | 800 | 100-1000 | Hz | fuse.h:12 | Accelerometer sampling rate |
| `GYRO_MEASUREMENTS_PER_SECOND` | 800 | 100-1000 | Hz | fuse.h:16 | Gyroscope sampling rate |
| `GYRO_STABILIZATION_TIME_MS` | 64 | 0-500 | ms | fuse.h:97 | Sensor warm-up time |

### Clash Detection Parameters

| Parameter | Default | Range | Units | Source | Description |
|-----------|---------|-------|-------|--------|-------------|
| `CLASH_THRESHOLD_G` | (user-defined) | 1.0-8.0 | G | prop_base.h:196 | Base clash sensitivity |
| `AUDIO_CLASH_SUPPRESSION_LEVEL` | 10 | 1-50 | - | prop_base.h:25 | Volume impact on threshold |

### Gesture Parameters

| Parameter | Default | Range | Units | Source | Description |
|-----------|---------|-------|-------|--------|-------------|
| Twist: min rotation rate | 200 | 100-500 | deg/s | prop_base.h:977 | Minimum twist speed |
| Twist: axis dominance | 3.0 | 2.0-5.0 | ratio | prop_base.h:978 | X-axis must dominate |
| Twist: min stroke time | 90 | 50-200 | ms | prop_base.h:992 | Shortest valid twist |
| Twist: max stroke time | 300 | 200-500 | ms | prop_base.h:993 | Longest valid twist |
| Twist: max separation | 200 | 100-400 | ms | prop_base.h:999 | Time between strokes |
| Shake: lateral threshold | 4 | 2-8 | m/s² | prop_base.h:1014 | Max Y/Z acceleration |
| Shake: forward threshold | 7 | 5-15 | m/s² | prop_base.h:1015 | Min forward thrust |
| Shake: backward threshold | -6 | -3 to -10 | m/s² | prop_base.h:1015 | Min backward thrust |
| Shake: max swing speed | 150 | 50-300 | deg/s | prop_base.h:1016 | Must not be swinging |
| Shake: stroke count | 5 | 3-7 | - | prop_base.h:1023 | Number of alternations |
| Shake: max separation | 250 | 150-500 | ms | prop_base.h:1030 | Time between strokes |
| Swing: start threshold | 250 | 150-500 | deg/s | prop_base.h:1044 | Begin swing detection |
| Swing: stop threshold | 100 | 50-200 | deg/s | prop_base.h:1048 | End swing detection |
| Stab: lateral ratio | 2.0 | 1.5-3.0 | - | prop_base.h:839 | Forward/lateral dominance |
| Stab: max swing speed | 150 | 50-300 | deg/s | prop_base.h:840 | Distinguish from clash |

### Spin Parameters

| Parameter | Default | Range | Units | Source | Description |
|-----------|---------|-------|-------|--------|-------------|
| `ProffieOSSpinDegrees` | 360.0 | 180-720 | degrees | hybrid_font.h:23 | Rotation to trigger spin |

### Fett263 Force Push Parameters

| Parameter | Default | Range | Units | Source | Description |
|-----------|---------|-------|-------|--------|-------------|
| `FETT263_FORCE_PUSH_LENGTH` | 5 | 1-10 | ms | saber_fett263_buttons.h:784 | Gesture duration |
| Force Push: max forward | 3.0 | 1-5 | m/s² | saber_fett263_buttons.h:2589 | Max longitudinal accel |
| Force Push: min lateral² | 100 | 50-200 | m/s⁴ | saber_fett263_buttons.h:2590 | Min lateral movement |
| Force Push: max swing | 20 | 10-50 | deg/s | saber_fett263_buttons.h:2591 | Max rotation rate |
| Force Push: max twist | 5 | 2-10 | deg/s | saber_fett263_buttons.h:2592 | Max twist rate |

### Fett263 Battle Mode Parameters

| Parameter | Default | Range | Units | Source | Description |
|-----------|---------|-------|-------|--------|-------------|
| `FETT263_LOCKUP_DELAY` | 200 | 50-500 | ms | saber_fett263_buttons.h:776 | Clash-to-lockup delay |
| `FETT263_BM_CLASH_DETECT` | 0 | 0-8 | G | saber_fett263_buttons.h:780 | Battle Mode 2.0 threshold |
| `FETT263_MAX_CLASH` | 16 | 8-16 | G | saber_fett263_buttons.h:937 | Max clash for sound selection |
| `FETT263_SWING_ON_SPEED` | 250 | 150-500 | deg/s | saber_fett263_buttons.h:772 | Swing-on ignition speed |

---

## Implementation Complexity

### Effect Complexity Ratings

| Effect | Rating | Reason |
|--------|--------|--------|
| **Swing** | ⭐ | Simple threshold + hysteresis |
| **Blade Angle** | ⭐⭐ | Trigonometry on gravity vector |
| **Spin** | ⭐⭐ | Angle accumulation + threshold |
| **Stab** | ⭐⭐ | Derived from clash detection |
| **Clash** | ⭐⭐⭐ | Dynamic thresholds, filtering, interrupt context |
| **Force Push** | ⭐⭐⭐ | Sustained gesture, multiple conditions |
| **Twist** | ⭐⭐⭐⭐ | Stroke state machine, timing windows |
| **Shake** | ⭐⭐⭐⭐ | Multi-stroke pattern matching |

### System Components

| Component | Complexity | CPU Usage | Dependencies |
|-----------|-----------|-----------|--------------|
| **Fusor** | ⭐⭐⭐⭐⭐ | High (continuous) | IMU hardware, extrapolators |
| **Clash Detection** | ⭐⭐⭐ | Medium (interrupt) | Fusor, audio mixer |
| **Gesture Detection** | ⭐⭐⭐⭐ | Low (event-driven) | Fusor, timers |
| **Lockup System** | ⭐⭐ | Negligible | None |
| **Blade Angle Functions** | ⭐⭐ | Low (per-frame) | Fusor |

### Resource Requirements

**Memory:**

- Fusor: ~200 bytes (filters + state)
- Gesture detection: ~80 bytes (stroke history)
- Lockup: ~4 bytes (state variables)

**CPU Overhead:**

- Fusor: ~5-10% (continuous sensor fusion at 800Hz)
- Clash detection: ~2% (runs in interrupt)
- Gesture detection: <1% (only when props call DetectX())
- Effects: <1% (event-driven)

**Latency:**

- Clash: 1-2ms (interrupt-driven)
- Gesture: 10-50ms (depends on gesture duration)
- Angle: 0ms (computed on-demand, cached per frame)

---

## Notes

**Coordinate System:**

- X-axis: Along blade (positive = toward tip)
- Y-axis: Perpendicular to blade (varies by hilt orientation)
- Z-axis: Perpendicular to blade (forms right-hand coordinate system)

**Units:**

- Acceleration: G (1 G = 9.80665 m/s²) or m/s² where noted
- Angular velocity: degrees/s
- Angles: radians (internal) or 0-32768 (style functions)

**Conditional Compilation:**

Many features require specific `#define` directives to be enabled:
- `ENABLE_SPINS` - Spin detection
- `FETT263_FORCE_PUSH` - Force push gesture
- `PROFFIEOS_DONT_USE_GYRO_FOR_CLASH` - Disable gyro in clash detection
- `AUDIO_CLASH_SUPPRESSION_LEVEL` - Audio-based clash adjustment

**Performance Considerations:**

- The Fusor runs continuously at 800Hz and is the most CPU-intensive component
- Clash detection runs in interrupt context and must be kept fast
- Gesture detection is event-driven and adds minimal overhead
- Effect complexity primarily impacts maintainability, not runtime performance

---

**Document Version:** 1.0
**Generated From:** ProffieOS source code (analyzed 2026-04-06)
**Applies To:** ProffieOS v7.x and later
