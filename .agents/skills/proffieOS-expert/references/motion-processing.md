# Motion Processing - Technical Specification

## Overview

This document details the motion processing system for lightsaber effects, covering accelerometer and gyroscope integration, filtering algorithms, and motion-based event detection.

## IMU Sensor Integration

### Supported Sensors

ProffieOS supports multiple IMU configurations:

| Sensor | Type | Interface | Sample Rate |
|--------|------|-----------|-------------|
| LSM6DS3H | 6-axis (accel + gyro) | I2C | 1660 Hz |
| MPU6050 | 6-axis (accel + gyro) | I2C | 1000 Hz |
| FXOS8700 + FXAS21002 | 6-axis (separate) | I2C | 800 Hz |

**Recommended for ESP32:** LSM6DS3H or MPU6050 (integrated 6-axis IMU)

### I2C Configuration

**LSM6DS3H:**
- **Address:** 0x6A (default) or 0x6B (SDO high)
- **Clock Speed:** 400 kHz (standard) or 1 MHz (fast mode)
- **Power Mode:** High performance
- **FIFO:** Optional (can batch samples)

**MPU6050:**
- **Address:** 0x68 (AD0 low) or 0x69 (AD0 high)
- **Clock Speed:** 400 kHz
- **Digital Low Pass Filter:** Config = 1 (~180 Hz bandwidth, 2ms delay)
- **Sample Rate Divider:** 0 (1 kHz output)

## Sampling Rates & Data Format

### Sample Rates

**High Performance (Proffieboard v2/v3 equivalent):**
- Accelerometer: 1600 Hz
- Gyroscope: 1600 Hz
- Total data rate: 3200 samples/second
- Interrupt-driven acquisition recommended

**Standard Performance (Proffieboard v1 equivalent):**
- Accelerometer: 800 Hz
- Gyroscope: 800 Hz
- Total data rate: 1600 samples/second

**Minimum Acceptable:**
- Both sensors: 400 Hz minimum
- Lower rates cause latency in swing detection

### Sensor Ranges

**Accelerometer:**
- **Recommended:** ±16g range (high impact tolerance)
- **Alternative:** ±8g range (higher precision)
- **Format:** 16-bit signed integer
- **Conversion:** `accel_g = raw_value × 16.0 / 32768.0`

**Gyroscope:**
- **Range:** ±2000 degrees/second
- **Format:** 16-bit signed integer
- **Conversion:** `gyro_dps = raw_value × 2000.0 / 32768.0`

### Data Format

```cpp
struct MotionData {
    int16_t accel_x;  // Raw accelerometer X
    int16_t accel_y;  // Raw accelerometer Y
    int16_t accel_z;  // Raw accelerometer Z
    int16_t gyro_x;   // Raw gyroscope X
    int16_t gyro_y;   // Raw gyroscope Y
    int16_t gyro_z;   // Raw gyroscope Z
};

struct Vec3 {
    float x, y, z;

    float len() {
        return sqrtf(x*x + y*y + z*z);
    }
};
```

## Axis Orientation

### Coordinate System

Standard lightsaber orientation:
- **X-axis:** Along blade (positive = toward tip)
- **Y-axis:** Perpendicular to blade (lateral)
- **Z-axis:** Perpendicular to blade (lateral, orthogonal to Y)

### Orientation Transformation

If your IMU mounting differs from standard orientation, apply rotation matrix:

```cpp
// Example: USB port toward blade
Vec3 transformed;
transformed.x = raw.y;
transformed.y = -raw.x;
transformed.z = raw.z;
```

Common orientations require pre-configured rotation matrices or quaternions.

## Filtering Algorithms

### 1. Box Filter (Moving Average)

**Purpose:** Smooth high-frequency noise from sensors

**Implementation:**
```cpp
template<typename T, int SIZE>
class BoxFilter {
private:
    T sum_;
    T data_[SIZE];
    int pos_;

public:
    T filter(T input) {
        sum_ -= data_[pos_];
        sum_ += input;
        data_[pos_] = input;
        pos_ = (pos_ + 1) % SIZE;
        return sum_ / SIZE;
    }
};
```

**Usage:**
```cpp
BoxFilter<Vec3, 3> gyro_filter;  // 3-sample average for SmoothSwing
BoxFilter<Vec3, 1> clash_filter;  // 1-sample (no filtering) for clash
```

**Characteristics:**
- **Latency:** (SIZE - 1) / (2 × sample_rate)
  - 3-sample @ 1600 Hz = 0.625 ms delay
- **Frequency Response:** Low-pass, rolls off at sample_rate / SIZE
- **Computational Cost:** Minimal (add, subtract, divide)

### 2. Extrapolator Filter (Linear Regression)

**Purpose:** Predict current value and rate of change using recent history

**Implementation:**
```cpp
template<typename T, int RATE>
class Extrapolator {
private:
    T data_[10];  // 10 samples
    int pos_;
    uint32_t last_time_;

    T avg_;
    T slope_;
    float avg_t_;
    uint32_t start_;

public:
    void push(T value, uint32_t timestamp) {
        data_[pos_] = value;
        pos_ = (pos_ + 1) % 10;

        // Calculate linear regression
        avg_ = sum(data_) / 10;

        float sum_t = 0, sum_t2 = 0;
        T sum_xt = 0;
        for (int i = 0; i < 10; i++) {
            float t = (timestamp - (10-i) * (1000000 / RATE));
            sum_t += t;
            sum_t2 += t * t;
            sum_xt += data_[i] * t;
        }

        avg_t_ = sum_t / 10;
        slope_ = (sum_xt - avg_ * sum_t) / (sum_t2 - sum_t * avg_t_);
        start_ = timestamp;
    }

    T get(uint32_t now) {
        float t = now - start_;
        return avg_ + slope_ * (t - avg_t_);
    }

    T slope() {
        return slope_;
    }
};
```

**Usage:**
```cpp
Extrapolator<Vec3, 1600> accel_extrapolator;
Extrapolator<Vec3, 1600> gyro_extrapolator;

// Every sample
accel_extrapolator.push(accel, micros());

// Get current extrapolated value
Vec3 predicted = accel_extrapolator.get(micros());
Vec3 velocity = accel_extrapolator.slope();
```

**Characteristics:**
- **Window Size:** 10 samples
- **Update Rate:** Sample rate / 20 (e.g., 80 Hz for 1600 Hz sampling)
- **Latency:** ~6.25 ms (10 samples @ 1600 Hz)
- **Output:** Current value + first derivative (slope)

### 3. Complementary Filter (Sensor Fusion)

**Purpose:** Combine accelerometer and gyroscope to track gravity vector

**Algorithm:**
```cpp
class Fusor {
private:
    Vec3 down_;  // Gravity vector in body frame
    Vec3 gyro_;  // Current gyroscope reading (deg/s)
    Vec3 accel_; // Current accelerometer reading (g)

public:
    void DoMotion(Vec3 gyro, uint32_t timestamp) {
        float delta_t = (timestamp - last_time_) / 1000000.0;  // seconds
        last_time_ = timestamp;

        // Gyroscope integration (quaternion rotation)
        Quat rotation = Quat(1.0, gyro * -(delta_t * M_PI / 180.0 / 2.0));
        rotation.normalize();
        down_ = rotation.rotate_normalized(down_);

        // Calculate accelerometer trust weight
        float wGyro = 1.0;
        wGyro += gyro.len() / 100.0;              // Distrust during rotation
        wGyro += fabs(accel_.len() - 1.0) * 50.0; // Distrust when not 1g
        wGyro += accel_slope.len() * 1000.0;      // Distrust during accel change

        // Complementary filter
        float gyro_factor = expf(logf(0.01) / wGyro);
        down_ = down_ * gyro_factor + accel_ * (1.0 - gyro_factor);
        down_.normalize();
    }
};
```

**Key Formula:**
```
gyro_factor = exp(ln(0.01) / weight)
fused = gyro_integrated × gyro_factor + accel × (1 - gyro_factor)
```

**Weight Adjustments:**
- Base: 1.0 (1% accel, 99% gyro when weight = 1)
- +gyro_speed / 100: Trust gyro more during fast rotation
- +|accel_magnitude - 1g| × 50: Trust gyro more when accel ≠ gravity
- +accel_change × 1000: Trust gyro more during acceleration

**Result:** Accurate "down" vector even during complex motion

## Swing Detection

### Swing Speed Calculation

**Formula:**
```cpp
float swing_speed() {
    return sqrtf(gyro_.y * gyro_.y + gyro_.z * gyro_.z);
}
```

**Why only Y and Z axes:**
- X-axis = twist (rotation along blade)
- Y & Z axes = swing (rotation perpendicular to blade)
- Pythagorean theorem gives total swing speed regardless of direction

**Pseudocode:**
```
FUNCTION calculate_swing_speed(gyro_y, gyro_z):
    swing_speed = SQRT(gyro_y² + gyro_z²)
    RETURN swing_speed  // degrees per second
END FUNCTION
```

### Swing Acceleration

**Formula:**
```cpp
float swing_accel() {
    Vec3 gyro_slope = gyro_extrapolator.slope();
    return sqrtf(gyro_slope.z * gyro_slope.z + gyro_slope.y * gyro_slope.y)
           * (M_PI / 180.0);  // Convert to radians/s²
}
```

**Purpose:** Detect aggressive/forceful swings (slashes)

### Swing Event Detection

**Thresholds:**
- **Swing Start:** 250 degrees/second
- **Swing End:** 100 degrees/second (hysteresis prevents flutter)

**State Machine:**
```
STATE: idle
  IF swing_speed > 250:
    STATE = swinging
    TRIGGER swing_event
  END IF
END STATE

STATE: swinging
  IF swing_speed < 100:
    STATE = idle
  END IF
END STATE
```

**Pseudocode:**
```
FUNCTION detect_swing():
    speed = calculate_swing_speed(gyro.y, gyro.z)

    IF NOT swinging AND speed > 250:
        swinging = TRUE
        trigger_event(EVENT_SWING)
    END IF

    IF swinging AND speed < 100:
        swinging = FALSE
    END IF
END FUNCTION
```

## Clash Detection

### Algorithm Overview

Clash detection combines accelerometer and gyroscope data with dynamic thresholds.

### Step 1: Calculate Clash Value

**Accelerometer Component:**
```cpp
Vec3 clash_mss() {
    Vec3 filtered_accel = clash_filter.get();
    Vec3 diff = filtered_accel - down_;  // Remove gravity
    return diff * 9.81;  // Convert g to m/s²
}
```

**Gyroscope Component:**
```cpp
float gyro_clash_value() {
    Vec3 filtered_gyro = gyro_filter.get();
    Vec3 extrapolated_gyro = gyro_extrapolator.get(current_time);
    Vec3 diff = filtered_gyro - extrapolated_gyro;
    return diff.len() / 9.81;  // Convert to equivalent m/s²
}
```

**Combined Clash Value:**
```cpp
Vec3 accel_clash = clash_mss();
float gyro_clash = gyro_clash_value();
float clash_value = (accel_clash.len() + gyro_clash) / 2.0;
```

**Why average both:**
- Accelerometer detects impact directly
- Gyroscope detects sudden rotation change
- Averaging improves accuracy and reduces false positives

### Step 2: Dynamic Threshold

**Base Threshold:** 1.0 to 4.5g (configurable)

**Dynamic Adjustments:**
```cpp
float threshold = CLASH_THRESHOLD_G * (1.0
    + gyro.len() / 500.0                    // Gyro compensation
    + audio_volume * 1e-9 * volume          // Audio compensation
);
```

**Gyro Compensation:**
- During fast swings, threshold increases
- At 500 deg/s: +1.0g added to threshold
- Prevents swing motion from triggering clashes

**Audio Compensation:**
- Speaker vibrations can trigger accelerometer
- Threshold increases with audio volume
- Default suppression level: 10

**Clash Detection:**
```cpp
if (clash_value > threshold) {
    float strength = clash_value;  // Store clash strength
    trigger_clash(strength);
}
```

### Step 3: Stab vs Clash Classification

**Stab Criteria:**
```cpp
bool is_stab = (diff.x < -2.0 * sqrt(diff.y*diff.y + diff.z*diff.z))
               && (swing_speed < 150);
```

**Conditions:**
1. Forward acceleration (X) > 2× lateral acceleration
2. Swing speed < 150 deg/s (not swinging)

**Result:** Triggers `STAB` event instead of `CLASH` event

### Pseudocode

```
FUNCTION detect_clash():
    // Get filtered data
    accel_clash = (filtered_accel - gravity) * 9.81
    gyro_clash = (filtered_gyro - extrapolated_gyro).length() / 9.81

    // Combined clash value
    clash_value = (accel_clash.length() + gyro_clash) / 2.0

    // Dynamic threshold
    threshold = BASE_THRESHOLD * (1.0 + gyro_speed / 500.0 + audio_factor)

    // Check threshold
    IF clash_value > threshold:
        // Classify stab vs clash
        IF accel_clash.x < -2.0 * SQRT(accel_clash.y² + accel_clash.z²):
            IF swing_speed < 150:
                trigger_event(EVENT_STAB, clash_value)
                RETURN
            END IF
        END IF

        trigger_event(EVENT_CLASH, clash_value)
    END IF
END FUNCTION
```

## Other Motion-Based Effects

### Twist Detection

**Purpose:** Detect rotation along blade axis (X-axis)

**Algorithm:**
```cpp
bool detect_twist() {
    float twist_speed = fabs(gyro_.x);
    float lateral_speed = max(fabs(gyro_.y), fabs(gyro_.z));

    return (twist_speed > 200.0) && (twist_speed > 3.0 * lateral_speed);
}
```

**Thresholds:**
- **Minimum:** 200 deg/s on X-axis
- **Dominance:** X-axis must be 3× larger than Y or Z

**Twist Direction:**
```cpp
enum TwistDirection {
    LEFT = -1,
    RIGHT = 1
};

TwistDirection get_twist_direction() {
    return (gyro_.x > 0) ? RIGHT : LEFT;
}
```

### Twist Gesture Recognition

**Pattern:** Left-Right-Left or Right-Left-Right

**Timing Requirements:**
- **Twist Duration:** 90-300 ms each
- **Separation:** < 200 ms between twists
- **Count:** 3 twists total

**State Machine:**
```
STATE: idle
  ON twist_detected:
    first_direction = current_direction
    count = 1
    STATE = expecting_second
    START timeout (200ms)
END STATE

STATE: expecting_second
  ON twist_detected:
    IF current_direction != first_direction:
      count = 2
      STATE = expecting_third
      START timeout (200ms)
    ELSE:
      RESET
    END IF
  ON timeout:
    RESET
END STATE

STATE: expecting_third
  ON twist_detected:
    IF current_direction == first_direction:
      TRIGGER twist_gesture
      RESET
    ELSE:
      RESET
    END IF
  ON timeout:
    RESET
END STATE
```

### Shake Detection

**Purpose:** Detect back-and-forth shaking motion

**Criteria:**
```cpp
bool detect_shake_stroke() {
    Vec3 mss = accel_ * 9.81;  // Convert to m/s²

    float lateral = sqrtf(mss.y*mss.y + mss.z*mss.z);
    float forward = mss.x;

    return (lateral < 4.0) &&          // Lateral accel < 4 m/s²
           (fabs(forward) > 6.0) &&    // Forward accel > 6 m/s²
           (swing_speed < 150);        // Not swinging
}
```

**Pattern Recognition:**
- **Strokes Required:** 5 alternating forward/backward
- **Separation:** < 250 ms between strokes
- **Direction Alternation:** Must alternate forward/backward

**Pseudocode:**
```
FUNCTION detect_shake():
    IF detect_shake_stroke():
        current_direction = SIGN(accel.x)

        IF current_direction != last_direction:
            stroke_count++
            last_direction = current_direction
            last_stroke_time = current_time

            IF stroke_count >= 5:
                trigger_event(EVENT_SHAKE)
                stroke_count = 0
            END IF
        END IF
    END IF

    IF (current_time - last_stroke_time) > 250ms:
        stroke_count = 0  // Reset on timeout
    END IF
END FUNCTION
```

### Blade Angle

**Purpose:** Measure blade pointing direction relative to gravity

**Algorithm:**
```cpp
float blade_angle() {
    return atan2f(down_.x, sqrtf(down_.z*down_.z + down_.y*down_.y));
}
```

**Range:**
- **-π/2** (-90°): Blade pointing down
- **0**: Blade horizontal
- **+π/2** (+90°): Blade pointing up

**Usage:** Can modulate blade color/brightness based on angle

### Spin Detection

**Purpose:** Track accumulated rotation during swinging

**Algorithm:**
```cpp
float accumulated_angle = 0;

void update_spin() {
    if (swinging) {
        accumulated_angle += swing_speed * delta_time;

        if (accumulated_angle > 360.0) {
            trigger_event(EVENT_SPIN);
            accumulated_angle -= 360.0;
        }
    } else {
        accumulated_angle = 0;
    }
}
```

**Default Threshold:** 360 degrees (one full rotation)

## Implementation Timing

### Main Loop Structure

```cpp
void motion_loop() {
    static uint32_t last_sample = 0;
    uint32_t now = micros();

    // Read sensor at configured rate (e.g., 1600 Hz = 625 μs)
    if ((now - last_sample) >= 625) {
        MotionData raw = read_imu();

        // Convert to physical units
        Vec3 accel = raw_to_accel(raw);
        Vec3 gyro = raw_to_gyro(raw);

        // Apply filters
        accel = accel_filter.filter(accel);
        gyro = gyro_filter.filter(gyro);

        // Update extrapolators
        accel_extrapolator.push(accel, now);
        gyro_extrapolator.push(gyro, now);

        // Sensor fusion
        fusor.DoMotion(gyro, now);
        fusor.DoAccel(accel, now);

        // Detect events (run at lower rate, e.g., 80 Hz)
        if ((now % 12500) == 0) {  // Every ~12.5ms
            detect_swing();
            detect_clash();
            detect_twist();
            detect_shake();
        }

        last_sample = now;
    }
}
```

### Performance Budget

**At 1600 Hz (625 μs per sample):**
- Sensor read (I2C): ~50 μs
- Filtering: ~20 μs
- Sensor fusion: ~100 μs
- **Total:** ~170 μs per sample (27% CPU @ 160 MHz)

**Event detection (80 Hz, 12.5 ms period):**
- All detections: ~50 μs
- **Total:** <1% CPU

## Summary of Key Parameters

| Parameter | Value | Purpose |
|-----------|-------|---------|
| **Sampling Rates** |
| Accelerometer | 800-1600 Hz | Motion capture |
| Gyroscope | 800-1600 Hz | Rotation capture |
| Motion processing | 80 Hz | Event detection |
| **Sensor Ranges** |
| Accelerometer | ±16g | Impact tolerance |
| Gyroscope | ±2000 deg/s | Fast rotation |
| **Swing Detection** |
| Swing start | 250 deg/s | Threshold |
| Swing end | 100 deg/s | Hysteresis |
| SmoothSwing start | 20 deg/s | Sensitive trigger |
| SmoothSwing max | 450 deg/s | Full volume |
| **Clash Detection** |
| Base threshold | 1.0-4.5g | Configurable |
| Gyro compensation | +speed/500 | Dynamic adjust |
| Stab threshold | forward > 2×lateral | Classification |
| Stab swing limit | < 150 deg/s | Must be still |
| **Twist Detection** |
| Minimum speed | 200 deg/s | On X-axis |
| Dominance ratio | 3:1 | X vs lateral |
| **Shake Detection** |
| Forward accel | > 6 m/s² | Stroke trigger |
| Lateral accel | < 4 m/s² | Isolation |
| Stroke count | 5 | Pattern match |
| Timeout | 250 ms | Between strokes |

## Next Steps

See detailed algorithm implementations in:
- **SmoothSwing:** `smoothswing-algorithm.md`
- **Audio System:** `audio-system.md`
- **Mixing Pipeline:** `audio-mixing-pipeline.md`
