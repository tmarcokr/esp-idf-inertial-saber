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

**Reason**: The InertialSwing engine loads audio files from `/sdcard/InertialFont/...`. The directory name `InertialFont` (13 chars) exceeds the FAT 8.3 filename limit. Without LFN enabled, the FAT VFS driver cannot resolve any path under this directory, causing `ESP_ERR_NOT_FOUND` on all `AudioChannel::load()` calls.

**Heap vs Stack**: LFN buffers are allocated on the heap (`CONFIG_FATFS_LFN_HEAP`) rather than the stack to avoid increasing stack requirements for tasks that perform file I/O.

---

_Add new overrides below this line following the same format._
