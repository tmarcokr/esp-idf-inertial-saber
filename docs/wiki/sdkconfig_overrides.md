# sdkconfig Overrides Registry

This document tracks all non-default `sdkconfig` modifications required by InertialSaber OS. Since `sdkconfig` is gitignored and regenerated on target changes (`idf.py set-target`), these overrides **must** be replicated in `sdkconfig.defaults` to persist across builds.

> [!IMPORTANT]
> After any `idf.py set-target` or `idf.py fullclean`, verify that all overrides listed here are present in the active `sdkconfig`. If using `sdkconfig.defaults`, they will be applied automatically.

> [!NOTE]
> **Existing local `sdkconfig` files (ESP-IDF 6.1).** ESP-IDF 6.1 marks every option that still holds its Kconfig default with a `# default:` line in `sdkconfig`. On the next build, options marked this way pick up the new value from `sdkconfig.defaults`. Options without the marker (set by hand or through `menuconfig`) keep their local value. This is how `CONFIG_FREERTOS_HZ` (§8) and the 2026-09-27 value of `CONFIG_ESP_MAIN_TASK_STACK_SIZE` (§9) reached existing local `sdkconfig` files without a manual edit; the 2026-10-02 value does not (see §9). If a local `sdkconfig` has a customised value for one of these keys, set the value listed here by hand (or with `idf.py menuconfig`).

---

## Override Log

### 1. FAT Long File Name Support

| Key | Default | Override | Since |
|---|---|---|---|
| `CONFIG_FATFS_LFN_HEAP` | `not set` | `y` | 2026-05-09 |
| `CONFIG_FATFS_MAX_LFN` | _(absent)_ | `255` | 2026-05-09 |
| `CONFIG_FATFS_LFN_NONE` | `y` | `not set` | 2026-05-09 |

**Reason**: Two critical subsystems depend on long filenames:
1. **Profile Loader** — `ProfileLoader::loadFromSd()` opens `profile.json` (4-char extension exceeds the 3-char 8.3 limit). Without LFN, the driver stores it as `PROFIL~1.JSO` and `fopen("profile.json")` silently fails. The system boots with zero profiles and the saber does not respond to any input — with **no error messages**.
2. **Audio Engine** — loads WAV files from paths like `/sdcard/profiles/inertial/...`. Directory names exceeding 8 characters (e.g. `InertialFont`) cannot be resolved without LFN.

**Heap vs Stack**: LFN buffers are allocated on the heap (`CONFIG_FATFS_LFN_HEAP`) rather than the stack to avoid increasing stack requirements for tasks that perform file I/O.

> [!CAUTION]
> This override is silently reset to `LFN_NONE` by `idf.py set-target` and `idf.py fullclean`. The failure mode is **silent** — no error logs, the saber simply does not ignite. Always verify after target changes.

---

### 2. PSRAM / SPIRAM Support (ESP32-S3)

| Key | Default | Override | Since |
|---|---|---|---|
| `CONFIG_SPIRAM` | `not set` | `y` | 2026-06-28 |
| `CONFIG_SPIRAM_MODE_OCT` | _(absent)_ | `y` | 2026-06-28 |
| `CONFIG_SPIRAM_SPEED_80M` | _(absent)_ | `y` | 2026-06-28 |

**Reason**: In Phase 6 (MemoryVfs + PSRAM Audio Preloading), latency-critical looping audio channels (`hum.wav` and the active swing pair) are preloaded into PSRAM to eliminate SD card read latency. This requires enabling SPIRAM support in ESP-IDF.
- **PSRAM Mode**: Octal mode (`CONFIG_SPIRAM_MODE_OCT`) is selected as it is standard for high-performance PSRAM modules (e.g. 8MB) on ESP32-S3 boards.
- **PSRAM Speed**: 80MHz (`CONFIG_SPIRAM_SPEED_80M`) ensures the memory bandwidth is maximized for the audio mixer.

> [!NOTE]
> These overrides are target-specific for ESP32-S3 and must be placed in `sdkconfig.defaults.esp32s3` to avoid target verification errors when building for the ESP32-C6.


### 3. ESP-IDF v6.1 Default Changes (No Override Required)

| Key | v5.4.4 Default | v6.1 Default | Action | Since |
|---|---|---|---|---|
| `CONFIG_LIBC_PICOLIBC` | _(absent, newlib)_ | `y` | None (default kept) | 2026-09-26 |
| `CONFIG_LIBC_PICOLIBC_NEWLIB_COMPATIBILITY` | _(absent)_ | `y` | None (default kept) | 2026-09-26 |
| `CONFIG_FATFS_USE_DYN_BUFFERS` | `not set` | `y` | None (default kept) | 2026-09-26 |
| `CONFIG_COMPILER_DISABLE_DEFAULT_ERRORS` | `y` | `not set` | None (default kept) | 2026-09-26 |

**Reason**: The migration from ESP-IDF v5.4.4 to v6.1 changed several defaults that affect this project. No `sdkconfig.defaults*` file was modified; the new defaults are accepted as-is:
1. **C library** — v6.1 builds with picolibc instead of newlib (newlib compatibility layer enabled).
2. **FATFS dynamic buffers** — FATFS sector buffers are now allocated separately from the filesystem structure.
3. **Default warnings as errors** — compiler default warnings are now treated as errors. New warnings must be fixed in code rather than silenced through this option.

The existing overrides (§1 FAT LFN, §2 PSRAM) still exist in v6.1 and apply unchanged.

> [!NOTE]
> Validated on the XIAO ESP32-S3 on 2026-09-26 (boot, SD, PSRAM preload, audio, IMU, LEDs, effects and profile cycle).

---

### 4. Flash Size (ESP32-S3)

| Key | Default | Override | Since |
|---|---|---|---|
| `CONFIG_ESPTOOLPY_FLASHSIZE_8MB` | `not set` (2 MB) | `y` | 2026-09-26 |

**Reason**: The target board, the Seeed XIAO ESP32-S3 Sense, carries an 8 MB flash chip (plus 8 MB octal PSRAM). With the 2 MB default, the bootloader reports `Detected size(8192k) larger than the size in the binary image header(2048k)` and limits flash access to 2 MB. The 8 MB setting is also compatible with 16 MB boards such as the ESP32-S3-DevKitC-1 N16R8 prototype, which simply use the first 8 MB. The partition table is unchanged (default single 1 MB app partition).

> [!NOTE]
> Target-specific: placed in `sdkconfig.defaults.esp32s3`.

---

### 5. Project Kconfig: Profile Parser Self-Test

| Key | Default | Override | Since |
|---|---|---|---|
| `CONFIG_SABER_PARSER_SELF_TEST` | _(new option)_ | `y` | 2026-09-26 |

**Reason**: A new project-owned `main/Kconfig.projbuild` ("InertialSaber" menu) replaces the previous `NDEBUG`-gated self-test with an explicit build option. When enabled, `ProfileParser::runSelfTest()` runs before hardware initialization in `SaberSystem.cpp`; a failing self-test aborts the boot with the error status. The option defaults to `y` so the default build keeps validating the parser at boot. Disable it (`# CONFIG_SABER_PARSER_SELF_TEST is not set`) in release builds to shorten boot time and save flash.

> [!NOTE]
> This is a project-defined option (not an ESP-IDF default), so it has no prior "default" value to compare against — it did not exist before this change.

---

### 6. Release Build Overlay (`sdkconfig.defaults.release`)

| Key | Default | Override | Since |
|---|---|---|---|
| `CONFIG_COMPILER_OPTIMIZATION_PERF` | `not set` (`-Og`, via `OPTIMIZATION_DEBUG`) | `y` (`-O2`) | 2026-09-26 |
| `CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_SILENT` | `not set` (`ASSERTIONS_ENABLE`) | `y` | 2026-09-26 |
| `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240` | `not set` (160 MHz) | `y` (240 MHz) | 2026-09-26 |
| `CONFIG_LOG_DEFAULT_LEVEL_WARN` | `not set` (INFO) | `y` | 2026-09-26 |
| `CONFIG_SABER_PARSER_SELF_TEST` | `y` (see §5) | `not set` | 2026-09-26 |
| `CONFIG_SABER_METRICS` | `not set` (see §7; `y` until 2026-10-02) | `not set` (explicit) | 2026-09-27 |

**Reason**: `sdkconfig.defaults.release` is an **optional overlay**, not part of the default build. It is layered on top of `sdkconfig.defaults` (and the target-specific `sdkconfig.defaults.esp32s3`, appended automatically) only when explicitly requested:

```bash
idf.py -B build_release -D IDF_TARGET=esp32s3 -D SDKCONFIG=build_release/sdkconfig \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.release" build
```

1. **`-O2` optimization** — trades the default `-Og` (debug-friendly) for full performance optimization in release builds.
2. **Silent assertions** — `assert()` and `configASSERT()` still abort the firmware, but without printing the failure message/expression, reducing flash usage. Left at the default (`ENABLE`, verbose) in the standard build.
3. **240 MHz CPU** — raises the default 160 MHz clock for maximum headroom in release builds.
4. **`LOG_DEFAULT_LEVEL_WARN`** — several components log at `INFO` on every play/stop event in real-time audio/visual paths; `WARN` avoids that overhead in release builds. Switch back to `INFO` if boot/profile-load messages are needed for field debugging.
5. **Parser self-test disabled** — see §5; skipped in release builds to shorten boot time.
6. **Metrics disabled** — see §7; the instrumentation compiles to nothing and `CONFIG_HEAP_USE_HOOKS` is no longer selected. Since 2026-10-02 this matches the Kconfig default; the overlay keeps the entry explicitly.

> [!NOTE]
> This overlay is opt-in via the build command above; it never affects the default `idf.py build` invocation or the standard `sdkconfig`.

---

### 7. Project Kconfig: Real-Time Metrics

| Key | Default | Override | Since |
|---|---|---|---|
| `CONFIG_SABER_METRICS` | _(new option)_ | `y` | 2026-09-27 |
| `CONFIG_SABER_METRICS` | `y` | `not set` (Kconfig `default n`; opt-in) | 2026-10-02 |
| `CONFIG_HEAP_USE_HOOKS` | `not set` | `y` (selected by `CONFIG_SABER_METRICS`, metrics builds only) | 2026-09-27 |

**Reason**: `CONFIG_SABER_METRICS` (menu "InertialSaber" in `main/Kconfig.projbuild`) enables the on-device metrics described in [Diagnostics](Diagnostics.md). It records bus cycle and effect `run()` timings, loop and IMU rates, event counters, heap allocations per CPU core, free heap and task stack high-water marks, and writes them to `/sdcard/metrics/session_NNN.csv` while the saber is retracted. The option defaulted to `y` until 2026-10-02. It now defaults to `n` (`default n` in `main/Kconfig.projbuild`), so metrics are opt-in and the default development build no longer produces the CSV. For a board test: enable the option with `idf.py menuconfig` → *InertialSaber* → *Record real-time metrics and write them to /sdcard/metrics*, flash, run the test, collect the CSV files, then disable the option again and rebuild.

`CONFIG_SABER_METRICS` uses `select HEAP_USE_HOOKS`, so one switch controls both options. `CONFIG_HEAP_USE_HOOKS` makes the heap call `esp_heap_trace_alloc_hook()` on every allocation. The project defines this hook in `main/system/metrics/MetricsAllocHook.cpp` (compiled only when both options are set) to count allocations per CPU core. No `sdkconfig.defaults*` entry is needed for `CONFIG_HEAP_USE_HOOKS`.

**Cost** (metrics builds only): about +11.4 KB flash, +0.8 KB internal RAM (`.bss`), a 4 KB reporter task stack and about 10 KB of PSRAM buffers.

The release overlay (§6) still disables the option explicitly (`# CONFIG_SABER_METRICS is not set`). All `SABER_METRIC_*` macros then compile to nothing and `CONFIG_HEAP_USE_HOOKS` returns to its default (`not set`). Check that no metrics code is left in the image with:

```bash
nm build_release/esp-idf-inertial-saber.elf | grep -c InertialSaber11Diagnostics   # 0 when metrics are off
```

> [!NOTE]
> This is a project-defined option. When it was added as a new symbol, its default applied to an existing local `sdkconfig` on the next build without a manual edit.

> [!IMPORTANT]
> **Default change on 2026-10-02 (ESP-IDF 6.1).** A local `sdkconfig` created while the default was `y` keeps `CONFIG_SABER_METRICS=y` through its `# default:` value tracking. Run `idf.py refresh-config --policy kconfig` once, or switch the option off in `idf.py menuconfig`.

---

### 8. FreeRTOS Tick Rate

| Key | Default | Override | Since |
|---|---|---|---|
| `CONFIG_FREERTOS_HZ` | `100` | `1000` | 2026-09-27 |

**Reason**: At 100 Hz every FreeRTOS timeout and delay is rounded to 10 ms ticks. At 1000 Hz they have 1 ms granularity:
1. **Bus fallback timeout** — the bus waits for a notification with a 10 ms timeout. At 100 Hz this became anything from 0 to 10 ms; it is now an exact 10 ms.
2. **IMU poll** — the IMU task's 20 ms poll timeout is now exact.
3. **PSRAM loader** — the loader's 10 ms close-wait poll is now exact.
4. **Short component delays** — `pdMS_TO_TICKS(x)` with `x < 10` no longer rounds to 0. For example, the 2 ms delay in `Mpu6050::resetFifo()` was a plain yield and is now a real 2 ms wait.
5. **Log timestamps** have 1 ms resolution.

The bus packet timestamps no longer depend on the tick: they come from `esp_timer` (see [SaberAction](SaberAction.md) §2.1).

**Cost**: one tick interrupt per millisecond per core, about 0.3–0.5 % CPU, and finer time slicing between ready tasks of equal priority. 1000 Hz is the highest rate ESP-IDF supports.

This entry also closes the earlier gap where the tick rate had no entry in this registry.

> [!NOTE]
> Placed in `sdkconfig.defaults` (all targets). The metrics boot block records the active value as `0,meta,tick_rate` (see [Diagnostics](Diagnostics.md)).

---

### 9. Main Task Stack

| Key | Default | Override | Since |
|---|---|---|---|
| `CONFIG_ESP_MAIN_TASK_STACK_SIZE` | `3584` | `6144` | 2026-09-27 |
| `CONFIG_ESP_MAIN_TASK_STACK_SIZE` | `6144` | `4096` | 2026-10-02 |

**Reason**: The main task runs the whole boot sequence: the profile parser self-test (§5), the SD profile scan, cJSON parsing, `std::string` work and the first profile load. A `-fstack-usage` estimate puts this path at about 3.6–4.3 KB against the 4 KB it had (3584 + 512), so the value was raised to 6144 on 2026-09-27. ESP-IDF adds `TASK_EXTRA_STACK_SIZE` (512 B) to the configured size, so the real stack was 6656 B. After the board measurement below, the value was lowered to 4096 on 2026-10-02: the real stack is now 4608 B.

**Cost**: boot only. `app_main` returns after start-up and the main task is deleted, which frees its stack.

> [!NOTE]
> **Measured on the board (2026-10-02).** The metrics boot block records the minimum free stack of the main task as `0,task,main,stack_free_min`. The board test measured 3356 B free on each of 3 boots against the 6656 B task stack (6144 + 512), so the boot sequence uses about 3.3 KB. With 4096 + 512 = 4608 B the expected headroom is about 1.3 KB, above the 1 KB `stack_margin_ge_1k` check in [Diagnostics](Diagnostics.md). Confirm it with `0,task,main,stack_free_min` at the next metrics board test.

> [!IMPORTANT]
> **Existing local `sdkconfig` keeps 6144.** The `CONFIG_ESP_MAIN_TASK_STACK_SIZE` line in a local `sdkconfig` created with the previous value has no `# default:` marker (it counts as user-set), so the new value from `sdkconfig.defaults` is not applied on the next build. Change it with `idf.py menuconfig` → *Component config* → *ESP System Settings* → *Main task stack size*, or edit the line to `CONFIG_ESP_MAIN_TASK_STACK_SIZE=4096`.

> [!NOTE]
> Placed in `sdkconfig.defaults` (all targets).

---

### 10. Project Kconfig: Audio Compressor Threshold

| Key | Default | Override | Since |
|---|---|---|---|
| `CONFIG_SABER_AUDIO_COMPRESSOR_THRESHOLD` | _(new option)_ | `1000` (Kconfig `default 1000`, range 600–2000) | 2026-10-02 |

**Reason**: `CONFIG_SABER_AUDIO_COMPRESSOR_THRESHOLD` (menu "InertialSaber" in `main/Kconfig.projbuild`) sets the baseline threshold of the dynamic range compressor in the audio mixer. It is read into `HardwareConfig::kAudioCompressorThreshold` (`main/system/hardware/HardwareConfig.hpp`) and passed to the audio engine as `AudioEngine::Config::compressor_gain_threshold`. The default keeps the previous hard-coded value (1000), so the default build sounds the same. The option allows loudness tuning by ear without code edits (planned trial: 1000 → 1100 → 1200) and tuning for other amplifiers than the MAX98357A.

**Semantics**: the compressor tracks the mean absolute level of the mixed signal (time constant about 256 samples, ≈5.8 ms) and applies gain = threshold / (√envelope + 100), capped at 1, where the envelope is a leaky sum of about 256 × the mean |sample|. Compression therefore starts when the mean |sample| exceeds (threshold − 100)² / 256:

| Threshold | Compression starts at mean \|sample\| | Approx. mean level |
|---|---|---|
| 1000 (default) | ≈3160 | ≈−20 dBFS |
| 1200 | ≈4730 | ≈−17 dBFS |

A higher value gives a louder output and starts compression later, at the cost of more risk of hard clipping at the int16 output clamp. The threshold is scaled by the mixer global volume; at the project's full global volume (`kAudioGlobalVolume = 16384`) it applies as configured.

To change it: `idf.py menuconfig` → *InertialSaber* → *Audio compressor gain threshold*, then rebuild and flash.

> [!NOTE]
> This is a project-defined option. As a new symbol, its default applies to an existing local `sdkconfig` on the next build without a manual edit. No `sdkconfig.defaults*` entry is needed while the default is used.

---
