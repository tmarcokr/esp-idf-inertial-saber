# Functional Specification: Profile System

## 1. System Overview
A **profile** defines the aesthetic, sonic and reactive identity of the saber: its sound font, its blade colour and the physics thresholds of its effects. Profiles are **data**, not code. Each profile is a folder on the SD card with a `profile.json` file and the WAV files of its sound font.

- At boot, the firmware scans `/sdcard/profiles/*/profile.json`, parses every file once and keeps the result as an immutable profile definition (`ConfigurableProfile`).
- The **active** profile is turned into a set of effects (`EffectSet`) that the [SaberAction Bus](SaberAction.md) evaluates every cycle.
- Profiles are hot-swappable while the blade is retracted: a triple click switches to the next one (§5).
- Nothing is compiled per profile. Adding, removing or editing a profile only needs the SD card.

| Part | Location | Role |
| :--- | :--- | :--- |
| `ProfileLoader` | `main/profiles/ProfileLoader.cpp` | Scans `/sdcard/profiles/` and reads every `profile.json`. |
| `ProfileParser` | `main/profiles/ProfileParser.cpp` | Parses and validates the JSON into an `InertialDefinition` (§4). |
| `ConfigurableProfile` | `main/profiles/ConfigurableProfile.cpp` | Immutable profile: definition + sound font. Builds a new `EffectSet` on request. |
| `SoundFont` | `main/profiles/SoundFont.cpp` | Normalises the font root and builds every WAV path without heap allocation. |
| `ProfileManager` | `main/profiles/ProfileManager.cpp` | Owns the profile list, the `profile_ctrl` task, the switch sequence and the delayed save. |
| `PsramAudioCache` | `main/system/PsramAudioCache.cpp` | Copies the active profile's `hum.wav` to PSRAM and serves it as `/mem/hum.wav`. |
| `ActiveProfileStore` | `main/system/persistence/ActiveProfileStore.cpp` | Reads and writes `/sdcard/active_profile.txt`. |

---

## 2. SD Card Layout

```text
/sdcard/
├── active_profile.txt          <-- index of the active profile (§6), written by the firmware
├── metrics/                    <-- metrics builds only (see Diagnostics.md)
└── profiles/
    └── <profile>/
        ├── profile.json        <-- profile definition (§4)
        ├── hum.wav             <-- looping hum, copied to PSRAM
        ├── font.wav            <-- played when the profile becomes ready
        ├── swingl/  swingl1.wav ... swinglN.wav   <-- low swing loops (paired with swingh/)
        ├── swingh/  swingh1.wav ... swinghN.wav   <-- high swing loops
        ├── swng/    swng1.wav ...                 <-- Inertial Burst one-shots
        ├── in/      in1.wav ...                   <-- ignition
        ├── out/     out1.wav ...                  <-- retraction
        ├── clsh/    clsh1.wav ...                 <-- clash
        ├── blst/    blst1.wav ...                 <-- blaster
        ├── drag/    drag1.wav ...                 <-- drag loop
        └── enddrag/ enddrag1.wav ...              <-- drag release
```

- The font folder is the one named by `root_path` (relative to `/sdcard/`). It is normally the profile folder itself (`"root_path": "profiles/<profile>/"`), but several profiles may share one font folder.
- Files are numbered from `1`. The number of files per folder comes from the `font_counts` section of `profile.json` (§4.4); a sound is picked at random among `1 … count`. Swing pair `i` is `swingl/swingl<i>.wav` + `swingh/swingh<i>.wav`.
- A missing or invalid file does not stop the profile: that sound does not play, an error is logged and the metrics counter `audio_play_failed` increases.
- The SD card must be **FAT32 with long file names**. The firmware needs LFN support (`CONFIG_FATFS_LFN_HEAP`, see [sdkconfig Overrides](sdkconfig_overrides.md) §1) to open `profile.json` and folder names longer than 8 characters.

An example profile is in `examples/sdcard/profiles/example/profile.json`. The `tools/create_profile.py` script builds the folder layout and a matching `profile.json` from a folder of WAV files.

### 2.1. WAV Format
Every sound must be a RIFF/WAVE file with **PCM** samples, **mono**, **16-bit**, **44 100 Hz**. The audio engine rejects any other format when the sound is played (an error is logged naming the file and the unsupported field). One second of audio takes 88.2 KB (44 100 × 2 B).

### 2.2. Profile Root Length Limit
The `root_path` of a profile is normalized first: leading and trailing `/` are removed and a single trailing `/` is added. The normalized `root_path` must be **at most 96 characters** long. Audio paths are built in a fixed 128-byte buffer (`/sdcard/` + root + the longest file name, `enddrag/enddrag255.wav`), so a longer root cannot be played.

A profile whose normalized `root_path` is longer than 96 characters is **rejected at parse time**: `ProfileParser::parse()` returns `ESP_ERR_INVALID_SIZE`, the error is logged with the actual length, and the profile is skipped during the SD scan. The other profiles load normally.

---

## 3. Discovery at Boot
1. `ProfileLoader` lists `/sdcard/profiles/` and opens `<entry>/profile.json` for every entry. Entries without a readable `profile.json` are skipped with a warning.
2. Each file is parsed once (§4). A file that is not valid JSON, or whose `root_path` is too long, is skipped with an error; the other profiles still load.
3. The profile order is the **directory order** returned by the SD card (FAT entry order), not an alphabetical order. The order is shown in the log (`LOADED profile '<name>'`) and decides which profile the next triple click selects.
4. `ProfileManager` restores the active index from `active_profile.txt` (§6) and loads that profile (§5.1).

If no valid profile is found, the boot stops: the log shows `No valid profile on SD` and `Each profile needs /sdcard/profiles/<name>/profile.json`, the status LED turns **solid red**, the action bus is not started and the saber does not react to the button. The usual causes are a missing `profiles/` folder, a card without long file name support or a JSON syntax error.

---

## 4. `profile.json` Schema
`profile.json` is one JSON object with two strings and seven sections. Every key is optional.

Validation rules (`ProfileParser.cpp`):
- A missing key, or a missing section, takes its **default**.
- A key that is not a number takes its default, and a warning is logged.
- A number outside the **range** is clamped to the nearest limit, and a warning is logged.
- Integer keys are truncated towards zero before the range check (`2.7` → `2`).
- Two pairs must be strictly increasing: `swing.max_threshold_g` > `swing.idle_threshold_g`, and `swing.crossfade_high_g` > `swing.crossfade_low_g`. Otherwise the higher key is raised to the lower one + 0.05 G, and a warning is logged.
- Unknown keys are ignored.

The G values are measured on the kinetic energy of the bus packet (see [Kinetic Metrics](KineticMetrics.md)). The accelerometer runs at ±2 g per axis, so thresholds near the top of the 16 G range are never reached. The defaults below are the tuned values of the current reference hardware; a calibration on an assembled hilt is planned after v1.0.

### 4.1. Top-Level Keys

| Key | Type | Default | Meaning |
| :--- | :--- | :--- | :--- |
| `name` | string | `"unnamed"` | Profile name shown in the log. |
| `root_path` | string | `"profiles/unnamed/"` | Font folder relative to `/sdcard/`. Normalised and limited to 96 characters (§2.2). |

### 4.2. `overload` — Inertial Overload Accumulator
See [Inertial Overload](InertialOverload.md). These four keys are applied by the bus when the profile becomes active.

| Key | Type | Default | Unit | Range | Meaning |
| :--- | :--- | ---: | :--- | :--- | :--- |
| `threshold_g` | float | 1.0 | G | 0 – 16 | Kinetic energy above which the accumulator charges. |
| `charge_rate` | float | 2.0 | 1/s | 0 – 100 | Charge per second while above the threshold (1.0 = full). |
| `drain_rate` | float | 0.5 | 1/s | 0 – 100 | Drain per second while at or below the threshold. |
| `burst_cooldown_ms` | float | 1500 | ms | 0 – 60000 | Time after an Inertial Burst during which the accumulator stays empty. |

### 4.3. `sensor` — Motion Deadbands
Applied by the bus to every packet before the effects see it.

| Key | Type | Default | Unit | Range | Meaning |
| :--- | :--- | ---: | :--- | :--- | :--- |
| `kinetic_deadband_g` | float | 0.25 | G | 0 – 2 | Kinetic energy below this value is reported as 0. |
| `rotation_deadband_dps` | float | 15.0 | °/s | 0 – 2000 | Each gyroscope axis below this absolute value is reported as 0. |

### 4.4. `swing` — InertialSwing and Clash
See [InertialSwing](InertialSwing.md) and [Kinetic Effects](KineticEffects.md) §2.1.

| Key | Type | Default | Unit | Range | Meaning |
| :--- | :--- | ---: | :--- | :--- | :--- |
| `idle_threshold_g` | float | 0.15 | G | 0 – 15.95 | Below this energy the swing volume is 0. |
| `max_threshold_g` | float | 1.0 | G | 0 – 16, > `idle_threshold_g` | At or above this energy the swing volume is at maximum. |
| `crossfade_low_g` | float | 0.4 | G | 0 – 15.95 | Below this energy the low swing (`swingl`) dominates. |
| `crossfade_high_g` | float | 1.0 | G | 0 – 16, > `crossfade_low_g` | Above this energy the high swing (`swingh`) dominates. |
| `gravity_influence` | float | 0.2 | — | 0 – 1 | Weight of the blade orientation on the low/high balance. |
| `hum_base_volume` | integer | 8000 | — | 0 – 16384 | Hum volume at rest (16384 = full scale). |
| `hum_max_ducking` | float | 0.75 | — | 0 – 1 | Hum reduction at full swing volume. |
| `swap_cooldown_ms` | integer | 1000 | ms | 0 – 60000 | Silence time after a swing before a new swing pair is chosen. |
| `swap_min_volume` | float | 0.40 | — | 0 – 1 | Swing volume a movement must exceed to make the next silence change the pair. |
| `clash_threshold_g` | float | 2.0 | G | 0.1 – 16 | Kinetic energy drop within 15 ms that counts as a clash. |

### 4.5. `font_counts` — Number of Files per Sound Folder

| Key | Type | Default | Unit | Range | Folder |
| :--- | :--- | ---: | :--- | :--- | :--- |
| `swing_pair` | integer | 3 | files | 0 – 19 | `swingl/` + `swingh/` pairs. `0`: no swing sound; `1`: the pair never changes. |
| `burst` | integer | 16 | files | 0 – 255 | `swng/`. `0`: the Inertial Burst plays no sound. |
| `in` | integer | 2 | files | 0 – 255 | `in/` |
| `out` | integer | 4 | files | 0 – 255 | `out/` |
| `blaster` | integer | 8 | files | 0 – 255 | `blst/` |
| `clash` | integer | 16 | files | 0 – 255 | `clsh/` |
| `drag` | integer | 1 | files | 0 – 255 | `drag/` |
| `drag_end` | integer | 4 | files | 0 – 255 | `enddrag/` |

The single `hum.wav` and `font.wav` need no count. For every folder except `swng/` and the swing pairs, a count of `0` behaves like `1` (file number 1 is tried).

### 4.6. `blade_timings` — Visual Durations

| Key | Type | Default | Unit | Range | Meaning |
| :--- | :--- | ---: | :--- | :--- | :--- |
| `ignition_duration_ms` | integer | 800 | ms | 1 – 30000 | Ignition sweep; the saber counts as ignited at its end. |
| `retraction_duration_ms` | integer | 500 | ms | 1 – 30000 | Retraction sweep. |
| `blaster_duration_ms` | integer | 250 | ms | 1 – 30000 | Blaster block flash. |
| `clash_duration_ms` | integer | 150 | ms | 1 – 30000 | Clash flash fade-out. |

### 4.7. `blade_leds` — Effect Sizes

| Key | Type | Default | Unit | Range | Meaning |
| :--- | :--- | ---: | :--- | :--- | :--- |
| `blaster_count` | integer | 3 | LEDs | 1 – 65535 | Length of the white blaster block (limited to the strip length). |
| `drag_count` | integer | 8 | LEDs | 1 – 65535 | Length of the drag glow at the blade tip (limited to the strip length). |

### 4.8. `light` — InertialLight
See [InertialLight](InertialLight.md).

| Key | Type | Default | Unit | Range | Meaning |
| :--- | :--- | ---: | :--- | :--- | :--- |
| `blade_base_hue` | integer | 240 | ° | 0 – 359 | Blade hue (0 red, 120 green, 240 blue). |
| `idle_base_freq` | float | 1.0 | Hz | 0.5 – 10 | Breathing rate with the blade horizontal. |
| `idle_pulse_depth` | float | 0.15 | — | 0 – 1 | Brightness depth of the breathing pulse. |
| `max_thermal_bleed` | float | 0.80 | — | 0 – 1 | Saturation lost at full Inertial Overload. |
| `flicker_intensity` | float | 0.20 | — | 0 – 1 | Brightness flicker caused by motion. |
| `burst_duration_ms` | integer | 150 | ms | 1 – 30000 | Plasma Rupture flash. |

The `idle_base_freq` minimum (0.5 Hz) equals the gravity modulation of the breathing rate, so the rate never reaches zero.

---

## 5. Loading and Switching

### 5.1. Boot Sequence
1. The active profile's effect set is built on the `main` task. Its state machine starts in **Locked**.
2. The copy of the profile's `hum.wav` to PSRAM is requested (§5.3), then the bus and the `profile_ctrl` task start.
3. While Locked, the status LED blinks yellow and the button is ignored for ignition.
4. When the copy completes, the state becomes **Retracted**, the status LED turns green and `font.wav` plays (from the SD card). The saber is ready.
5. If the copy fails (missing `hum.wav`, read error, not enough PSRAM), the state becomes **Faulted** and the status LED turns solid red. Ignition stays disabled; a triple click retries by switching to the next profile (with a single profile it reloads the same one).

### 5.2. Profile Switch
A **triple click** while the saber is Retracted or Faulted switches to the next profile in scan order (wrapping around).

1. The state becomes **Switching**: ignition is blocked. The bus only raises a request flag; it does no allocation and no SD access.
2. The `profile_ctrl` task (core 1) builds the next profile's effect set, requests its hum copy and hands the set to the bus.
3. At the top of its next cycle the bus commits the new set with a pointer swap (O(1), no allocation; metric `profile_commit`) and hands the old set back. `profile_ctrl` destroys the old set off the bus.
4. The new set starts Locked, exactly as at boot (§5.1 steps 3–5). The switch ends with `font.wav` of the new profile.

Triple clicks are ignored while a switch is pending and while the new profile is Locked, so switches never overlap. The bus is never paused by a switch.

**Switch time.** Building and committing the set take a few milliseconds; the switch time is dominated by the copy of the new `hum.wav`, so it grows with the hum size. The copy rate is reported by the loader's INFO log line (`Preload gen N complete: … kB/s`) and by the metrics rows `preload_copy` and `preload_kBps` ([Diagnostics](Diagnostics.md)). Measured values are pending board validation.

### 5.3. Audio Storage and the PSRAM Budget
- **Only `hum.wav` is held in PSRAM**, as `/mem/hum.wav`. The hum loops for the whole time the blade is lit, so it is read from memory instead of the SD card.
- **Every other sound streams from the SD card**: all swing pairs, all one-shots (`in/`, `out/`, `clsh/`, `blst/`, `drag/`, `enddrag/`, `swng/`) and `font.wav`. All the files listed in `font_counts` are used; nothing is dropped to save memory.
- The copy runs on the `psram_loader` task (core 1) in 32 KB chunks through an internal DMA buffer, so each SD read is short.
- Before the copy, the previous profile's hum is released. If an audio channel still holds it open, the loader waits up to 1 s for it to close (up to 3 s in total when the new hum does not fit otherwise).
- **Size limit**: `hum.wav` must fit in the largest free PSRAM block minus 256 KB of headroom. The log line of a failed copy shows the hum size, the largest block and the free PSRAM. The free PSRAM at boot is logged and, in metrics builds, recorded as `boot,heap_psram_free`.
- Because swings and one-shots stream from the card, a fast card is recommended: **Class 10 / U1** (A1 preferred). Up to 8 sounds can stream from the SD card at the same time while the hum plays from PSRAM.

---

## 6. Active Profile Persistence
- `/sdcard/active_profile.txt` holds the zero-based index of the active profile in scan order (§3). A missing or unreadable file selects index 0; an index beyond the number of profiles is reset to 0 with a warning.
- After a switch, `profile_ctrl` saves the new index about **1.5 s** later, and only while the saber is Retracted or Faulted and no hum copy is in progress. If the saber is ignited first, the save waits until the blade is retracted, then waits another 1.5 s. Switching again before the save restarts the delay; switching back to the saved profile cancels it.
- **Power-loss semantics**: if power is cut within about 1.5 s of a switch, or while the saber is lit before the first retraction after a switch, the previous profile is restored at the next boot. Wait a few seconds with the saber retracted before cutting power to keep the new profile.
- The index refers to a position, not a name: adding, removing or renaming a profile folder can change which profile an index selects.

---

## 7. Effects of a Profile
Every profile builds the same eight effects, each bound to that set's own power state machine (`PowerStateMachine`). Their evaluation order is listed in [SaberAction](SaberAction.md) §4.1.

| Effect | Role |
| :--- | :--- |
| `PreloadWaitEffect` | Locked → Retracted (or Faulted) when the hum copy ends; plays `font.wav`. |
| `InertialSwingEffect` | Hum and swing mixer ([InertialSwing](InertialSwing.md)). |
| `InertialLightEffect` | Blade colour and brightness ([InertialLight](InertialLight.md)). |
| `PowerToggleEffect` | Ignition and retraction. |
| `DragEffect` | Drag while the button is held. |
| `ProfileCycleEffect` | Profile switch on a triple click. |
| `BlasterEffect` | Blaster on a single click while ignited. |
| `KineticImpactEffect` | Clash on a kinetic energy drop. |

The button gestures are described in [SaberAction](SaberAction.md) §2.2 and the effect triggers in [Kinetic Effects](KineticEffects.md).
