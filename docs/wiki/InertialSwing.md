# Technical Specification: InertialSwing Engine

## 1. System Overview
The **InertialSwing Engine** is a high-performance audio model designed for the ESP32-S3. This engine is based on **Linear Kinetic Energy** and **Physical Inertia**, allowing the saber to react to any movement (steps, thrusts, spins) in an organic and cinematic way. It is implemented by `InertialSwingEffect` (`main/profiles/inertial/effects/`), a flow modulator evaluated on every bus cycle while the blade is ignited.

The engine is divided into three subsystems:
1. **Inertial Crossfade**: Controls the volume and base mix according to the force of the movement.
2. **Gravity Tonal Modulator**: Adjusts the "color" of the sound according to the orientation of the saber.
3. **Zero-Volume Swing Pair Swapper**: Changes the swing pair during silence, without audible noise or latency.

Every threshold below is a per-profile key of the `swing` section of `profile.json` (defaults in [Profiles](Profiles.md) §4.4).

---

## 2. Energy and Mix Calculation (Inertial Crossfade)
The engine uses the kinetic energy of the bus packet: the magnitude of the linear acceleration with gravity removed (see [Kinetic Metrics](KineticMetrics.md) §1).

**Energy:**
`kineticEnergy = sqrt(lin_x² + lin_y² + lin_z²)` in G, reported as 0 below `sensor.kinetic_deadband_g` (default 0.25 G).

### 2.1. Intensity Thresholds
The `kineticEnergy` defines the master volume and the balance between the `swingL` (Low/Bass) and `swingH` (High/Treble) files. Values for the default profile keys:

| State | Energy (G), defaults | Keys | Mixer Action |
| :--- | :--- | :--- | :--- |
| **Idle (Calm)** | below 0.15 G | `idle_threshold_g` | Only Hum audible. Swing Volume = 0%. |
| **Rising** | 0.15 – 1.0 G | `idle_threshold_g` … `max_threshold_g` | Master swing volume rising linearly. |
| **L Zone** | below 0.4 G | `crossfade_low_g` | `swingL` dominates. |
| **Transition** | 0.4 – 1.0 G | `crossfade_low_g` … `crossfade_high_g` | Crossfade active: `swingL` goes down and `swingH` goes up. |
| **H Zone** | above 1.0 G | `crossfade_high_g` | `swingH` dominates; master volume at maximum from `max_threshold_g`. |

With the default deadband (0.25 G) the effective idle limit is 0.25 G.

---

## 3. Gravity Modulation (Gravity Tonal Modulator)
The orientation of the blade (`orientation`, −1.0 … +1.0) acts as an equalizer that "colors" the sound, giving more weight to one texture or another depending on the direction. With the default `gravity_influence` of 0.2:

* **Saber Up (+1.0):** Ethereal sound. Injects +20% weight to `swingH`.
* **Saber Horizontal (0.0):** Neutral tone defined only by Intensity.
* **Saber Down (−1.0):** Heavy/roaring sound. Injects +20% weight to `swingL`.

**Final Mix Calculation:**
1. `GravityModulator = sin(orientation × π/2)`
2. `BaseMix = clamp((kineticEnergy − crossfade_low_g) / (crossfade_high_g − crossfade_low_g), 0.0, 1.0)`
3. `FinalMix = clamp(BaseMix + (GravityModulator × gravity_influence), 0.0, 1.0)`

---

## 4. File Management (Zero-Volume Swing Pair Swapper)
Only one pair of swing files (`swingl<i>.wav` / `swingh<i>.wav`) plays at a time. Both files of a pair are looped and **streamed from the SD card**; only `hum.wav` is held in PSRAM (`/mem/hum.wav`, see [Profiles](Profiles.md) §5.3). The pair is changed in the "Invisible Moment", while the swing volume is zero.

- **Start:** on ignition, the hum starts and a random pair is started at volume 0.
- **Linked start:** the two files of a pair are prepared and started together by one `AudioEngine::playLinked()` call (queued to the `audio_ctrl` task), so they begin in the same mixer cycle and stay sample-aligned. If one of them runs short of buffered data, the mixer holds both (metrics counter `audio_group_holds`).
- **Swap trigger:** a movement whose master volume exceeded `swap_min_volume` (default 0.40) arms a swap. When the master volume has returned to 0 and stayed there for `swap_cooldown_ms` (default 1000 ms), a new pair is chosen at random among the other `font_counts.swing_pair` pairs and started linked at volume 0, replacing the previous pair.
- **No swap** with `swing_pair` ≤ 1. With `swing_pair` = 0 the swing layer is silent.
- **Failure:** if the swap command cannot be queued (`audio_commands_dropped`), the playing pair is kept. If the new files cannot be started (for example, a missing file), the old pair has already been stopped and the swing layer stays silent until the next swap (`audio_play_failed`).
- **Cost on the bus:** the swap builds its paths without heap allocation and only queues one command; the SD file opens run on core 1.

### 4.1. Flowchart

```text
[ CALM ] ──────( Master volume > 0 )───────────────┐
  Volume: 0%                                       │
  Pair: ready                                      ▼
      ▲                                      [ MOVEMENT ]
      │                                       Volume: > 0%
      │                                       Mix active
      │                                       (> swap_min_volume arms a swap)
  ( INVISIBLE MOMENT )                             │
  1. Volume == 0 for swap_cooldown_ms              ▼
  2. Pick another random pair              [ FADE OUT ]
  3. playLinked(L, H) at volume 0           Volume drops to 0%
     (replaces the previous pair)                  │
  4. State -> CALM                                 │
      ▲                                            │
      └───────────( Volume == 0.0 )────────────────┘
```

> [!NOTE]
> **Swing pair source and phase.** Since the SD streaming change, the swing pairs are no longer cached in PSRAM. The pair is started sample-aligned by the linked start described above. The by-ear comparison of SD-streamed and PSRAM-cached swing pairs (A/B listening test) is **pending board validation**.

---

## 5. Mixer Integration (Final Formulas)

For implementation on the ESP32-S3, the volume of each channel is calculated in each bus cycle (about 155 Hz on average, measured on the reference board, debug build) following this order of precedence:

### 5.1. Master Swing Volume Calculation
Determines the global presence of the movement sound based on total inertia.
`MasterVolume = clamp((kineticEnergy − idle_threshold_g) / (max_threshold_g − idle_threshold_g), 0.0, 1.0)`

### 5.2. Tonal Balance (Inertial Crossfade + Gravity)
Calculates the proportion between the two files of the playing pair.
1. `BaseMix = clamp((kineticEnergy − crossfade_low_g) / (crossfade_high_g − crossfade_low_g), 0.0, 1.0)`
2. `GravityMod = sin(orientation × π/2) × gravity_influence`
3. `FinalMix = clamp(BaseMix + GravityMod, 0.0, 1.0)`

### 5.3. Output per Channel
`Volume_SwingL = MasterVolume × (1.0 − FinalMix) × 16384`
`Volume_SwingH = MasterVolume × FinalMix × 16384`
`Volume_Hum = hum_base_volume × max(0, 1.0 − MasterVolume × hum_max_ducking)`

16384 is the full-scale channel volume.

### 5.4. Inertial Burst
When the bus reports an Inertial Burst (`inertialBurst`), the engine plays a random `swng/` one-shot at full volume (none if `font_counts.burst` is 0). See [Inertial Overload](InertialOverload.md).

---

## 6. Tuning Notes

| Parameter (`swing.*`) | Default | Description |
| :--- | :--- | :--- |
| `idle_threshold_g` / `max_threshold_g` | 0.15 / 1.0 G | Energy range of the master swing volume. |
| `crossfade_low_g` / `crossfade_high_g` | 0.4 / 1.0 G | Energy range of the low/high crossfade. |
| `gravity_influence` | 0.2 (20%) | How much tilt affects the L/H balance. |
| `hum_base_volume` / `hum_max_ducking` | 8000 / 0.75 | Hum level at rest and its reduction at full swing. |
| `swap_min_volume` / `swap_cooldown_ms` | 0.40 / 1000 ms | Swap arming level and the silence needed before a swap. |
