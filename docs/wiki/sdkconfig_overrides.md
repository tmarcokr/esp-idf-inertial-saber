# sdkconfig Overrides Registry

This document tracks all non-default `sdkconfig` modifications required by InertialSaber OS. Since `sdkconfig` is gitignored and regenerated on target changes (`idf.py set-target`), these overrides **must** be replicated in `sdkconfig.defaults` to persist across builds.

> [!IMPORTANT]
> After any `idf.py set-target` or `idf.py fullclean`, verify that all overrides listed here are present in the active `sdkconfig`. If using `sdkconfig.defaults`, they will be applied automatically.

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

