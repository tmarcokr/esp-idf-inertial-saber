# Technical Specification: On-Device Diagnostics (Real-Time Metrics)

## 1. Overview
InertialSaber OS can measure its own real-time behaviour on the board and write the results to the SD card as CSV files. The metrics cover:
- bus cycle and effect `run()` timings, plus a bus cycle histogram;
- bus loop and IMU sample rates;
- event counters (dropped inputs, dropped audio commands, IMU poll timeouts, bursts, etc.);
- heap allocations counted per CPU core, free heap and the largest free block;
- stack high-water mark, priority and core of every task.

Recording uses 32-bit atomics in internal RAM (under 1 KB). It never allocates heap, takes a lock or blocks, so it is safe on the bus task and the audio tasks. A low-priority reporter task (`metrics`, priority 1, core 1) aggregates the data and writes it to the SD card only while the saber is retracted and the audio is idle.

| Part | Location | Role |
| :--- | :--- | :--- |
| Recording facade | `main/diagnostics/Metrics.{hpp,cpp}` | `SABER_METRIC_*` macros and the live store. Usable from `main/core/`. |
| Reporter task | `main/system/metrics/MetricsReporter.{hpp,cpp}` | Samples heap, stacks and rates, captures one block per session and writes the CSV. |
| Allocation hook | `main/system/metrics/MetricsAllocHook.cpp` | `esp_heap_trace_alloc_hook()`: counts every heap allocation on the calling CPU core. |

---

## 2. Enabling and Disabling
Metrics are controlled by one Kconfig option, `CONFIG_SABER_METRICS` (menu "InertialSaber"). See [sdkconfig Overrides](sdkconfig_overrides.md) §7.

| Build | `CONFIG_SABER_METRICS` | Result |
| :--- | :--- | :--- |
| Default (`idf.py build`) | `not set` (default) | Every `SABER_METRIC_*` macro compiles to nothing; its arguments are not evaluated. No reporter task, no allocation hook. |
| Metrics build (enabled in `menuconfig`) | `y` | Metrics recorded and written to the SD card. Also selects `CONFIG_HEAP_USE_HOOKS`. |
| Release overlay (`sdkconfig.defaults.release`) | `not set` (explicit) | Same as the default build. |

Metrics are opt-in. To toggle them in a local build, use `idf.py menuconfig` → *InertialSaber* → *Record real-time metrics and write them to /sdcard/metrics*.

For a board test: enable the option, flash, run the test, collect the CSV files, then disable the option again and rebuild.

> [!NOTE]
> With ESP-IDF 6.1, an existing local `sdkconfig` created while the default was `y` keeps `CONFIG_SABER_METRICS=y` through its `# default:` value tracking. Run `idf.py refresh-config --policy kconfig` once, or switch the option off in `menuconfig`. See [sdkconfig Overrides](sdkconfig_overrides.md) §7.

To confirm that a build carries no metrics code:

```bash
nm build_release/esp-idf-inertial-saber.elf | grep -c InertialSaber11Diagnostics   # 0 when metrics are off
```

---

## 3. Files and Blocks

### 3.1. File naming
- Directory: `/sdcard/metrics/` (created on the first write).
- File: `session_NNN.csv`, **one file per power-up**. `NNN` is the highest existing index plus one, found when the first block is written. The index is capped at `999`; after that, `session_999.csv` is reused (appended to) and a warning is logged.
- If the SD card cannot be accessed, the reporter logs one warning and retries silently.

### 3.2. Blocks
Each file is a sequence of blocks. Every row starts with its block number.

| Block | Content | Captured |
| :--- | :--- | :--- |
| **0** | Build metadata and boot data (`meta`, `boot`, main task stack). | At the end of the boot sequence, except `boot,imu_settle_ms`, which is known only when the IMU settles. Written at the first idle opportunity once `imu_settle_ms` is known (or 50 s after power-up), even if the saber is never ignited. Session blocks are written after it. |
| **k ≥ 1** | Everything recorded in the **window since the previous retraction** (for block 1: since the reporter started at the end of boot). | When a retraction completes (`Retracting → Retracted`). |

A block window therefore includes the retracted time before the ignition, so profile cycles and PSRAM preloads are measured too. Two rows separate the two times:
- `session,window` — the whole block window, in ms.
- `session,on_time` — the time from the accepted ignition request to the completed retraction, in ms.

> [!NOTE]
> The live accumulators are first reset when block 1 is captured. Block 1 therefore also contains the durations and counters recorded during boot before the reporter started (for example the first bus cycles). Its `window`, rates and allocation totals start at the end of boot.

### 3.3. When blocks are written
Blocks are captured in RAM (a ring of 4 pending blocks in PSRAM) and written later, when all of these hold:
1. The saber is retracted (no active session).
2. At least **2 s** have passed since the last retraction (for block 0: since the reporter started).
   Block 0 also waits until the IMU has switched to the DMP values, at most until 50 s after power-up. The 50 s cover the 40 s settling backstop (counted from the first IMU sample) plus the boot time.
3. The audio output has been idle (output level ≤ 140) for at least **500 ms**. The threshold sits above the small residual the mixer keeps outputting in silence: it is twice the residual bound of the configured DC-blocker cutoff, so it follows the cutoff preset.
4. No PSRAM preload is in progress (at boot and after every profile change), no profile change is pending and no active-profile save is pending or being written, so the metrics never compete with them for the SD card.

Right before each block is written, the reporter re-checks that no ignition has started and that condition 4 still holds; if not, the remaining blocks stay pending and are written at a later opportunity. Each block is written with its own open → append → close, so the data already written survives a power cut. After each write attempt ends, the next one waits another 2 s.

> [!IMPORTANT]
> **Wait 5 s after retracting before cutting power.** Otherwise the last block (or block 0, right after boot) may not be written yet.

**Status LED signal.** Every write attempt is shown on the status LED: solid **blue** while the write is in progress, then, counted from the end of the write, fast **blue** blinking (100 ms half-period) for 1 s if it succeeded, or fast **red** blinking for 2 s if the file could not be opened or the write failed. If an ignition starts before any block is written, the solid blue ends without a blink. The LED then returns to the current system status. No blue or red signal within ~5 s of a retraction means the reporter never attempted the write.

If the saber is ignited again before the pending blocks are written, they stay in the ring. When the ring already holds 4 blocks, the next retraction does not capture a new block (see `lost_before`, §4.2).

### 3.4. Comparing files
Row order is fixed (enum order in the code), so two files diff line by line. The results depend on the build flavour (optimisation level, CPU frequency, log level, tick rate). **Compare only CSV files whose block 0 `meta` rows show the same build flavour.**

---

## 4. CSV Format

### 4.1. Columns
Long format, one value per row, no commas inside values:

```text
block,section,name,stat,value,unit
```

| Column | Meaning |
| :--- | :--- |
| `block` | Block number (0 = boot, then 1, 2, … per session). |
| `section` | Row group: `meta`, `boot`, `session`, `rate`, `time`, `hist`, `worst`, `derived`, `count`, `heap`, `task`, `alloc`, `check`. |
| `name` | Metric, counter, task or check name. |
| `stat` | Statistic (`count`, `avg`, `max`, `min`, …); empty when the row has a single value. |
| `value` | Integer, one-decimal number, text, `PASS`/`FAIL` or `missing`. |
| `unit` | `us`, `ms`, `Hz`, `MHz`, `B`, `mG` or empty. |

The header line is written with block 0, once per file.

### 4.2. Block 0 rows

| Row (`section,name,stat`) | Unit | Meaning |
| :--- | :--- | :--- |
| `meta,schema,` | — | CSV schema version (currently `1`). |
| `meta,fw_version,` | — | Application version from the image descriptor (`git describe` at build time). |
| `meta,build_time,` | — | Build date and time. |
| `meta,idf_version,` | — | ESP-IDF version. |
| `meta,elf_sha256,` | — | First 16 hex characters of the ELF SHA-256; identifies the exact binary. |
| `meta,cpu_freq,` | MHz | `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ`. |
| `meta,tick_rate,` | Hz | `CONFIG_FREERTOS_HZ`. |
| `meta,log_level,` | — | `CONFIG_LOG_DEFAULT_LEVEL` (0 none … 3 info … 5 verbose). |
| `meta,optimization,` | — | `debug`, `perf`, `size` or `none`. |
| `boot,duration,` | ms | Time from power-up to the end of `SaberSystem::start()`. |
| `boot,reset_reason,` | — | Cause of the current boot (`esp_reset_reason()`): `POWERON`, `EXT`, `SW`, `PANIC`, `INT_WDT`, `TASK_WDT`, `WDT`, `DEEPSLEEP`, `BROWNOUT`, `SDIO`, `USB`, `JTAG`, `EFUSE`, `PWR_GLITCH`, `CPU_LOCKUP`, `UNKNOWN`, or the numeric value for a reason not in this list. In a battery test, anything other than `POWERON` (or `USB`/`JTAG` right after flashing) means an unexpected reboot. |
| `boot,heap_internal_free,` | B | Free internal heap at the end of boot. |
| `boot,heap_psram_free,` | B | Free PSRAM heap at the end of boot. |
| `boot,imu_settle_ms,` | ms | Time from power-up to the IMU sample where the motion values switched from the start-up fallback to the DMP (see `imu_fallback_samples`). `-1`: the DMP gravity did not match the accelerometer within 4° for 20 consecutive quasi-static samples during the first 20 s of IMU samples; the ceiling then forced the switch at the next quasi-static sample. `-2`: no quasi-static sample arrived between the 20 s ceiling and the 40 s backstop, so the switch was forced at the next sample, moving or not (for example, a gyro zero-rate offset above 20 °/s). `missing`: no switch within 50 s of power-up (no IMU samples, or a very slow boot). |
| `task,main,stack_free_min` | B | Minimum free stack of the main task over the whole boot sequence. |

### 4.3. Session block rows (k ≥ 1)
Rows appear in this order.

**Session**

| Row | Unit | Meaning |
| :--- | :--- | :--- |
| `session,window,` | ms | Length of the block window (§3.2). |
| `session,on_time,` | ms | Time spent from ignition request to completed retraction. |
| `session,lost_before,` | — | Sessions since the previous block that have no block of their own. A session is counted when it ended while 4 blocks were already pending (its durations, counters, `on_time` and heap data are merged into this block), or when an earlier block failed to write (its data is lost). Expected `0`. |

**Rates** — `rate,bus,*` and `rate,imu,*`

| Stat | Unit | Meaning |
| :--- | :--- | :--- |
| `min` / `max` | Hz | Lowest / highest rate over the complete 1 s windows inside the block (retracted and ignited time). `0` if no full window was completed. |
| `avg` | Hz | Total count over the whole block window, with one decimal. |

`bus` counts bus cycles; `imu` counts IMU samples delivered to the bus.

**Timings** — `time,<metric>,<stat>`, in microseconds (except `ke_quasi_static_settled`, in milli-g), one group per metric in this order:

| Metric | Kind | What it measures |
| :--- | :--- | :--- |
| `bus_cycle` | scope | Work of one bus cycle: packet build, motion and overload update, input drain and every `test()`/`run()`. Followed by its histogram (see below). |
| `bus_interval` | interval | Time between the starts of consecutive bus cycles. |
| `run_preload_wait`, `run_swing`, `run_light`, `run_power_toggle`, `run_blaster`, `run_clash`, `run_drag`, `run_profile_cycle` | scope | One effect's `run()`, only when its `test()` returned true. |
| `swing_activate` | scope | `InertialSwingEffect::activate()` (queues the hum and swing voices on ignition). |
| `swing_swap` | scope | The zero-volume swing pair swap. |
| `imu_read` | scope | One IMU FIFO read in the IMU task. |
| `audio_play_call` | scope | One `AudioEngine::play()` call in the `audio_ctrl` task. |
| `audio_latency` | duration | From the moment an effect queues a play command to the end of the `play()` call. |
| `motion_age` | duration | Age of the IMU sample when the bus cycle uses it (bus cycle time − `motionTimestampUs`). |
| `profile_commit` | scope | The bus taking over a staged profile effect set at the top of a cycle (pointer swap and physics update). Must not allocate; counted in `bus_loop_allocs`. |
| `profile_build` | scope | Building the next profile's effect set in the `profile_ctrl` task (allocations expected, on core 1). |
| `profile_save` | scope | Writing the active profile index to the SD card in the `profile_ctrl` task. |
| `profile_switch` | duration | From the accepted profile-cycle request to the end of the new profile's preload, when the saber unlocks. Not recorded at boot or when the preload fails. |
| `ke_quasi_static_settled` | duration | Kinetic energy in **mG** of every IMU sample taken while the saber is quasi-static (accelerometer magnitude within 0.08 g of 1 g and every gyro axis below 20 °/s) after the switch to the DMP values. `max` is the largest one: a high value means fake energy from an unsettled DMP. |
| `audio_play_call_sd` | duration | The `AudioEngine::play()` call alone, for a path outside `/mem/`: every one-shot and swing pair, streamed from the SD card. |
| `audio_play_call_mem` | duration | The `AudioEngine::play()` call alone, for a path under `/mem/`: the hum, cached in PSRAM. |
| `preload_copy` | duration | Copy of the profile's `hum.wav` from the SD card to PSRAM by the `psram_loader` task (chunked reads through the internal DMA buffer), from the first read to the last byte. Not recorded for a copy that failed or was superseded. |

| Kind | Stats written |
| :--- | :--- |
| scope | `count`, `avg`, `max`, `allocs`, `scopes_with_alloc` |
| duration | `count`, `avg`, `max` |
| interval | `avg`, `max` |

- `allocs`: heap allocations counted on the scope's CPU core while the scope was open (see §5).
- `scopes_with_alloc`: number of scope executions with at least one allocation.
- `avg` is an integer (`sum / count`). The sum is a 32-bit µs value per block, so a metric whose sum grows with wall time (`bus_interval`) wraps if a block window exceeds about 71 minutes.

**Histogram** — `hist,bus_cycle,<bucket>`: number of bus cycles per duration bucket: `lt_250us`, `lt_500us`, `lt_1ms`, `lt_2ms`, `lt_5ms`, `lt_10ms`, `lt_20ms`, `lt_50ms`, `ge_50ms`.

**Worst and derived values**

| Row | Unit | Meaning |
| :--- | :--- | :--- |
| `worst,run,name` | — | The `run_*` metric with the highest `max` (`none` if no effect ran). |
| `worst,run,max` | us | Its `max`. |
| `derived,bus_loop_allocs,` | — | `bus_cycle.allocs` − the sum of all `run_*.allocs` (floored at 0): allocations made by the bus loop itself or by `test()` methods. |
| `derived,preload_kBps,` | kB/s | SD → PSRAM copy rate: `preload_bytes` × 1000 / the sum of the `preload_copy` durations (1 kB = 1000 B). `missing` when the block has no `preload_copy` sample. |

**Counters** — `count,<name>,`

| Counter | Meaning |
| :--- | :--- |
| `bus_timeout_wakes` | Bus cycles started by the 10 ms fallback timeout instead of a notification. |
| `input_events_dropped` | Input events lost because the bus input queue was full. |
| `imu_samples` | IMU samples read and staged to the bus. |
| `imu_empty_reads` | IMU reads that returned no sample. |
| `imu_poll_timeouts` | IMU task wakes by its 20 ms poll timeout instead of the data-ready interrupt. |
| `overlays_dropped` | LED overlays rejected because no overlay slot was free. |
| `bus_cycles` | Bus cycles executed. |
| `audio_commands_dropped` | Audio commands lost because the `audio_ctrl` queue was full. |
| `audio_play_failed` | Play requests with an invalid or truncated path, or that the audio engine rejected (no channel). |
| `inertial_bursts` | Inertial Bursts fired by the Overload accumulator. |
| `clash_detections` | Clashes detected (each one plays a clash sound and flash). |
| `clash_retrigger_lt_1s` | Clash detections that followed the previous detection by less than 1000 ms. |
| `imu_fallback_samples` | IMU samples delivered with the start-up fallback values (kinetic energy `\| \|a\| − 1 g \|` and roll from the accelerometer tilt) because the DMP had not settled yet. |
| `preload_bytes` | Bytes copied from the SD card to PSRAM by the completed `preload_copy` samples. |

**Heap** — sampled at ignition, every 100 ms while ignited, and at retraction. Dips shorter than 100 ms can be missed.

| Row | Unit | Meaning |
| :--- | :--- | :--- |
| `heap,internal_free,start\|min\|end` | B | Free internal heap at ignition / lowest sample / at retraction. |
| `heap,internal_largest_block,min` | B | Smallest largest-free-block of internal heap (fragmentation). |
| `heap,internal_min_since_boot,` | B | Lowest free internal heap since boot, as tracked by the heap (exact). |
| `heap,psram_free,start\|min\|end` | B | Same as `internal_free`, for PSRAM. |

**Tasks** — `task,<name>,<stat>` for every task, in this order: `main`, `saber_bus`, `imu_adapter`, `psram_loader`, `metrics`, `SmartLedTask`, `gpio_btn_tsk`, `esp_timer`, `audio_mixer`, `audio_sd_reader`, `audio_mem_reader`, `audio_ctrl`, `profile_ctrl`.

| Stat | Unit | Meaning |
| :--- | :--- | :--- |
| `size` | B | Stack size from the task table in `HardwareConfig.hpp` (for `main` and `esp_timer`, including the 512 B ESP-IDF adds). |
| `stack_free_min` | B | Minimum free stack since the task was created (high-water mark). |
| `priority` | — | Task priority when the block was captured. |
| `core` | — | `0`, `1`, or `any` for unpinned tasks. |

- `main`: the task is deleted after boot, so its `stack_free_min`, `priority` and `core` are the values captured at the end of boot.
- `missing`: the task handle is unknown (for example, a component task that did not exist when the reporter looked it up by name). `size` is still written.

**Allocation totals** — `alloc,core0,` and `alloc,core1,`: all heap allocations made on each CPU core during the block window.

**Checks** — `check,<name>,` = `PASS` or `FAIL`:

| Check | PASS when |
| :--- | :--- |
| `bus_cycle_max_lt_2ms` | `time,bus_cycle,max` < 2000 µs. |
| `bus_loop_allocs_zero` | `derived,bus_loop_allocs` = 0. |
| `flow_effects_allocs_zero` | `run_swing.allocs` + `run_preload_wait.allocs` = 0 **and** `run_light.scopes_with_alloc` ≤ `count,inertial_bursts` (each burst may allocate once for the Plasma Rupture overlay). |
| `stack_margin_ge_1k` | Every task with a known handle has `stack_free_min` ≥ 1024 B (`missing` tasks are skipped). |

---

## 5. Interpreting Allocation Counts
The allocation hook runs inside every heap allocation. It cannot identify the calling task safely (that call is not IRAM-resident), so it counts allocations **per CPU core**. A scope's `allocs` value is the difference of its core's counter between scope entry and exit. It therefore includes allocations by any other task that ran on the same core while the scope was open (for example the IMU task or `esp_timer` callbacks preempting the bus on core 0).

The counts are an **upper bound**:
- `0` / `PASS` is conclusive: the scope made no allocation.
- A non-zero value / `FAIL` may include allocations by other tasks on the same core. Check `alloc,core0|core1` and the other `time,*,allocs` rows before blaming the effect.

**Accepted trigger-time allocations.** These happen once per trigger, never in the steady-state loop, and are visible as `run_*.allocs`:
- SmartLed overlays (`pushOverlay()` takes a heap-allocated effect): blaster, clash, ignition and retraction sweeps, drag, Plasma Rupture flash;
- the drag overlay's fade flag;
- InertialLight activation (the base blade effect);
- profile swaps (profile unload/load).

---

## 6. Snapshot Accuracy
A block is captured by exchanging every live accumulator with zero, one value at a time, while the real-time tasks keep recording. A sample recorded at that exact moment can be split between two blocks (for example its `count` in block k and its time sum or `max` in block k+1). At most one sample per metric is affected, which is negligible for sessions of normal length.

---

## 7. Implementation Notes
- Recording macros: `SABER_METRIC_SCOPE`, `SABER_METRIC_DURATION`, `SABER_METRIC_INTERVAL`, `SABER_METRIC_COUNT`, `SABER_METRIC_ADD`, `SABER_METRIC_REGISTER_TASK`, `SABER_METRIC_RECORD_BOOT`, `SABER_METRIC_SESSION_BEGIN`, `SABER_METRIC_SESSION_END`.
- Session boundaries come from `PowerStateMachine::handle()`: an accepted ignition request starts a session; a completed retraction ends it.
- The reporter uses about 10 KB of PSRAM (block ring and an 8 KB text buffer) and a 4 KB stack. Values are formatted as integers only.
