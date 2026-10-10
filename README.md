# esp-idf-inertial-saber (InertialSaber OS)

**InertialSaber OS** is a high-performance, open-source operating system for lightsabers, specifically engineered for the ESP32-S3. InertialSaber OS utilizes a **Native Physics Engine** to deliver an organic, high-fidelity experience driven by real-time kinetic data.

## 🚀 The Core Philosophy: "Inertial Logic"

The system is built on the principle that a lightsaber should feel like a contained column of plasma reacting to physical laws. Every sound and flash is a direct mathematical result of:
- **Linear Kinetic Energy (G-Force)**
- **Inertial Overload & Burst (Kinetic Accumulation)**
- **Gravitational Orientation (Spatial Awareness)**

## 🛠️ System Architecture

InertialSaber OS is modular and extensible, organized into specialized engines and management systems:

### 1. [InertialSwing Engine](./docs/wiki/InertialSwing.md)
The audio core. It replaces standard "swing sounds" with a physics-based mixer that calculates volume and tonal balance based on the kinetic energy applied to the hilt.

### 2. [InertialLight Engine](./docs/wiki/InertialLight.md)
The visual core. Working in HSB (Hue, Saturation, Brightness) space, it simulates plasma behavior. From the "Live Breathing" pulse at rest to the "Thermal Bleed" (whitening) during high-speed movement and "Plasma Rupture" flashes.

### 3. [Inertial Overload Accumulator](./docs/wiki/InertialOverload.md)
The "Inertia Tank". A physics-driven accumulator that tracks sustained movement intensity. Once fully charged, it triggers **Inertial Bursts**, creating a bridge between continuous motion and explosive climax events.

### 4. [SaberAction System (The Action Bus)](./docs/wiki/SaberAction.md)
The asynchronous event dispatcher. It wakes on every IMU sample (100 Hz) and on a 10 ms timeout fallback — about 155 Hz on average, measured on the reference board (debug build) — and evaluates the active effects (clash, blaster, drag, ignition, …) against each snapshot of motion and button data.

### 5. [Profile System](./docs/wiki/Profiles.md)
Data-driven profiles: each one is a `profile.json` file plus a sound font on the SD card, parsed once at boot. A triple click switches to the next profile while the blade is retracted; the switch is prepared off the real-time path and committed in one step.

### 6. [InertialHaptics Engine](./docs/wiki/InertialHaptics.md) (roadmap)
A planned tactile engine (plasma mass, friction, recoil). **Not implemented**: there is no haptic hardware support yet.

**Implemented effects**: ignition and retraction, swing (hum + crossfaded swing pairs), Inertial Burst, clash, blaster, drag, profile switch. **Roadmap**: lockup, stab, motion gestures and haptics ([Kinetic Effects](./docs/wiki/KineticEffects.md), [Kinetic Gestures](./docs/wiki/KineticGestures.md)).

## 🔩 Hardware

v1.0 supports one board: the **ESP32-S3-DevKitC-1 with the WROOM-1-N16R8 module** (16 MB flash, 8 MB octal PSRAM). The firmware uses 8 MB of flash by design.

### Bill of Materials

| Part | Notes |
| :--- | :--- |
| ESP32-S3-DevKitC-1 **N16R8** | 8 MB octal PSRAM is required (the hum is held in PSRAM). The on-board BOOT button is the main button; the on-board RGB LED is the status LED. |
| MPU-6050 module (GY-521) | I2C at 400 kHz, address 0x68, powered from 3V3. The INT pin must be connected. |
| MAX98357A I2S amplifier breakout | VIN from the 5 V rail. GAIN to GND through 100 kΩ (15 dB). |
| Speaker, 4 Ω 3 W | On OUT+/OUT− only (bridged output, never to GND). |
| microSD breakout, **passive** (no level shifter) | Powered from 3V3. 5 V "SPI SD modules" with buffers do not work in SDMMC mode. |
| microSD card, Class 10 / U1 (A1 preferred) | FAT32, see [SD Card](#-sd-card). |
| WS2812 (GRB) LED strip | 5 LEDs in v1.0 (`kNumLeds` in `main/system/hardware/HardwareConfig.hpp`). |
| 74AHCT125 level shifter, 330–470 Ω resistor, 470–1000 µF capacitor | Blade data line at 5 V, series resistor at the first LED, bulk capacitor at the strip power input. |
| 2 × 10 kΩ resistors | SD CMD and D0 pull-ups to 3V3 (and DAT1–3 if the breakout exposes them). |
| Battery with a 5 V boost converter, ≥ 2 A | Supplies the board (5V pin), the amplifier and the strip. |
| Optional momentary switch | External main button from GPIO0 to GND, in parallel with BOOT. |

### Pin Map

The table matches `main/system/board/BoardDevKitC1.hpp` (selected by `Board.hpp`).

| Signal | GPIO | Peripheral | Connects to | Notes |
| :--- | ---: | :--- | :--- | :--- |
| Main button | 0 | GPIO input, pull-up, active low | On-board BOOT (optional external switch to GND) | Strapping pin: holding it at power-on or reset enters download mode. |
| IMU SDA | 4 | I2C, 400 kHz | MPU-6050 SDA | Internal pull-ups on; module pull-ups expected. |
| IMU SCL | 5 | I2C | MPU-6050 SCL | Twist with GND, keep under 20 cm. |
| IMU INT | 6 | GPIO input, rising edge | MPU-6050 INT | Must be connected. |
| SD D0 | 7 | SDMMC 1-bit, 20 MHz | microSD DAT0 | 10 kΩ pull-up to 3V3. |
| SD CMD | 8 | SDMMC | microSD CMD | 10 kΩ pull-up to 3V3. |
| SD CLK | 9 | SDMMC | microSD CLK | Keep under 10 cm. |
| I2S BCLK | 11 | I2S TX, 44.1 kHz 16-bit | MAX98357A BCLK | |
| I2S WS | 12 | I2S TX | MAX98357A LRC | |
| I2S DOUT | 13 | I2S TX | MAX98357A DIN | |
| Amp enable | 14 | GPIO output | MAX98357A SD | Low (amplifier off) until the audio engine is ready. |
| Blade data | 21 | RMT, WS2812 | First LED DIN through the level shifter and the series resistor | |
| Status LED | 48 | RMT, WS2812 | On-board RGB LED | Initial DevKitC-1 revision. On v1.1 boards the RGB LED is on GPIO38, so the status LED stays dark (the firmware still runs). |

Do not connect: GPIO3, 45, 46 (strapping), 19/20 (USB), 43/44 (UART console), 26–37 (flash and octal PSRAM; 35–37 are on the header but used by the PSRAM).

### Wiring and Power Notes
- Feed the 5 V rail from the battery boost to the DevKitC **5V** pin, the amplifier VIN and the strip. USB alone cannot drive the audio.
- Never connect USB and the battery 5 V at the same time unless through a Schottky diode; open the battery switch before flashing.
- Use a star ground at the battery/boost ground; power the blade straight from the 5 V rail.
- MAX98357A: 220–470 µF + 100 nF at VIN; solder the connections. An optional 10 kΩ from SD to GND avoids a pop at power-up.
- microSD: 10 µF + 100 nF at its 3V3; keep CLK/CMD/D0 short.
- MPU-6050: short twisted wires, away from the speaker and the blade power.
- Do not hold the button while switching on (download mode). If the saber seems dead, release it and reset or power-cycle.
- Resets during ignition usually mean a supply dip (brownout). A metrics build records the cause as `boot,reset_reason` ([Diagnostics](./docs/wiki/Diagnostics.md)).

## 🧰 Getting Started

### 1. Install ESP-IDF v6.1
Install **ESP-IDF v6.1** with the ESP-IDF Installation Manager (EIM) or with the Espressif **ESP-IDF extension for VS Code** (listed in `.vscode/extensions.json`). See the [ESP-IDF Get Started guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html). Run the commands below in a terminal where the ESP-IDF environment is active (the "ESP-IDF Terminal" of the extension, or the activation script installed by EIM).

### 2. Build and Flash

```bash
git clone https://github.com/tmarcokr/esp-idf-inertial-saber.git
cd esp-idf-inertial-saber
idf.py set-target esp32s3          # once; regenerates sdkconfig from sdkconfig.defaults
idf.py build
idf.py -p <PORT> erase-flash       # recommended before the first flash (custom partition table)
idf.py -p <PORT> flash monitor
```

`<PORT>` is the serial port of the DevKitC-1 (for example `/dev/ttyUSB0` or `COM5`). Exit the monitor with `Ctrl+]`.

### 3. Build Configurations

| Build | Command | Use |
| :--- | :--- | :--- |
| Default (development) | `idf.py build` | `-Og`, assertions, 160 MHz, INFO logs, parser self-test at boot. |
| Release | `idf.py -B build_release -D IDF_TARGET=esp32s3 -D SDKCONFIG=build_release/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.release" build` | `-O2`, 240 MHz, WARN logs, no self-test. Flash with `idf.py -B build_release -p <PORT> flash`. |
| Metrics | `idf.py -B build_metrics -D IDF_TARGET=esp32s3 -D SDKCONFIG=build_metrics/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.metrics" build` | Default build plus on-device metrics written to `/sdcard/metrics/` ([Diagnostics](./docs/wiki/Diagnostics.md)). Flash with `idf.py -B build_metrics -p <PORT> flash monitor`. |

Every `sdkconfig` option the project changes is listed in [sdkconfig Overrides](./docs/wiki/sdkconfig_overrides.md). CI builds the three configurations on every push and publishes them as artifacts (`firmware-esp32s3-<config>`); to flash an artifact, extract it and run `python -m esptool --chip esp32s3 write-flash @flash_args` inside the extracted folder.

## 💾 SD Card

- Format the card as **FAT32** (32 KB clusters recommended). The firmware uses long file names.
- Use a **Class 10 / U1** card (A1 preferred): every sound except the hum streams from the card while the blade is lit.
- Each profile is a folder under `/profiles/` with a `profile.json` and its sound font:

```text
/profiles/<profile>/
    profile.json
    hum.wav       font.wav
    swingl/swingl1.wav …   swingh/swingh1.wav …   swng/swng1.wav …
    in/in1.wav …   out/out1.wav …   clsh/clsh1.wav …   blst/blst1.wav …
    drag/drag1.wav …   enddrag/enddrag1.wav …
```

- **WAV format**: PCM, **mono**, **16-bit**, **44.1 kHz**. Other formats are rejected when played.
- The number of files per folder is set in `profile.json` (`font_counts`). Every key, default and range is documented in [Profiles](./docs/wiki/Profiles.md) §4.
- A complete example is in [`examples/sdcard/`](./examples/sdcard/) (`profiles/example/profile.json`; no audio files are shipped).
- [`tools/create_profile.py`](./tools/create_profile.py) builds the folder layout and a matching `profile.json` from a folder of WAV files and checks their format:

```bash
python3 tools/create_profile.py <wav_folder> <sd_root>/profiles <profile_name> <blade_hue>
```

The firmware writes `active_profile.txt` (the active profile) to the card root, and metrics builds write `metrics/`.

## 🎮 Controls

One button (BOOT, GPIO0). A click is reported 400 ms after the last press; a press held 500 ms is a hold.

| Action | When | Result |
| :--- | :--- | :--- |
| 1 click | Retracted (status LED green) | Ignite |
| 2 clicks | Ignited | Retract |
| 1 click | Ignited | Blaster |
| Hold (≥ 500 ms) | Ignited | Drag while held; released → drag end |
| 3 clicks | Retracted, or after a failed hum load (red, see [Troubleshooting](#-troubleshooting)) | Next profile |

- A press released between 400 and 500 ms still counts as a click (it fires on release). A hold never fires a blaster.
- Space consecutive blasters **more than 400 ms apart**: two presses within 400 ms are a double click and retract the blade.
- Clashes are detected from motion (a sudden kinetic energy drop) while ignited; no button needed.
- Button input is ignored during the ignition and retraction sweeps and while a profile is loading.

## 🚦 Status LED

| LED (on-board RGB, GPIO48) | Meaning |
| :--- | :--- |
| Solid yellow | Booting. |
| Blinking yellow | Loading the profile's hum into PSRAM. Ignition is locked. |
| Solid green (dim) | Ready. Stays green while ignited. |
| Solid red | Error: SD card, IMU or audio initialisation failed, no valid profile was found, or the profile's hum could not be loaded. See the serial log. |
| Solid blue, then fast blue blinking for 1 s | Metrics builds only: a metrics block is being written / was written. |
| Fast red blinking for 2 s | Metrics builds only: a metrics write failed. |

## 🔁 Profile Switching

- At boot the active profile's `hum.wav` is copied to PSRAM (blinking yellow); then `font.wav` plays and the saber is ready (green).
- A triple click while retracted switches to the next profile in SD directory order. The new profile is built off the real-time path; ignition stays locked until its hum is copied, then its `font.wav` plays. The switch time is about the time to copy the new `hum.wav` from the card.
- Only `hum.wav` is held in PSRAM; it must fit in the free PSRAM (several MB on an N16R8). Every other sound streams from the SD card.
- The active profile is saved about 1.5 s after the switch, never while the blade is lit. If power is cut before the save, the previous profile comes back at the next boot.

Details: [Profiles](./docs/wiki/Profiles.md) §5–§6.

## 🩺 Troubleshooting

| Symptom | Likely cause and fix |
| :--- | :--- |
| Red LED at boot, log `No valid profile on SD` | No `/profiles/<name>/profile.json` on the card, a JSON syntax error (`PARSE FAILED` in the log), or long file names not available (see [sdkconfig Overrides](./docs/wiki/sdkconfig_overrides.md) §1). Also check that `root_path` is at most 96 characters. |
| Red LED at boot, log `SD Card init failed` | Card not inserted or not FAT32, wiring of CLK/CMD/D0, missing 10 kΩ pull-ups on CMD and D0, or a non-passive SD module. |
| Red LED after a profile switch, log `Preload gen N failed` | `hum.wav` missing, unreadable or too large for the free PSRAM (`Not enough PSRAM`). Triple click to go to the next profile. |
| The saber does nothing after power-on, log `waiting for download` | The button was held at power-on (GPIO0 is a strapping pin). Release it and reset. |
| A sound never plays | Log `Unsupported …`: the WAV is not PCM mono 16-bit 44.1 kHz. Log `Failed to open file`: the file is missing; files are numbered from 1 up to the count in `font_counts`. |
| No sound at all | Amplifier VIN not on 5 V (USB alone is not enough), SD_MODE (GPIO14) not wired, or speaker wired to GND. |
| Swing or effect sounds stutter | Slow SD card: use a Class 10 / U1 card. A metrics build counts the gaps as `audio_underrun_samples`. |
| The profile is not remembered after power-off | Power was cut within about 1.5 s of the switch, or while the blade was still lit. Retract and wait a few seconds. |
| Status LED stays dark | v1.1 DevKitC-1 boards have their RGB LED on GPIO38. The firmware still works. |
| Log `SD_HOST: input line delay not supported`, or a flash-size notice at boot | Expected on this board; harmless. |

## 📂 Project Structure

- `main/`: Core application logic — action bus (`core/`), profiles and effects (`profiles/`), adapters, audio control, board pin map and hardware layer (`system/`), on-device real-time metrics (`diagnostics/`).
- `components/`: Hardware wrappers and peripheral drivers (vendored from an upstream repository; do not edit here).
- `docs/wiki/`: Official functional and technical specifications (The Source of Truth).
- `examples/sdcard/`: Example SD card profile.
- `tools/`: Host tools (`create_profile.py`).
- `sdkconfig.defaults*`, `partitions.csv`: Build defaults, release and metrics overlays, and the 8 MB partition table.

## 📑 Documentation Index (Wiki)

Explore the technical depth of InertialSaber OS:
1. [**System Overview**](./docs/wiki/Home.md) - The "Big Picture" and data flow.
2. [**Kinetic Metrics**](./docs/wiki/KineticMetrics.md) - Understanding Energy, Overload, and Orientation.
3. [**Inertial Overload**](./docs/wiki/InertialOverload.md) - Detailed physics of the accumulation tank.
4. [**Audio Specification**](./docs/wiki/InertialSwing.md) - Deep dive into the InertialSwing Engine.
5. [**Visual Specification**](./docs/wiki/InertialLight.md) - Deep dive into the InertialLight Engine.
6. [**Haptic Specification**](./docs/wiki/InertialHaptics.md) - Roadmap concept for the InertialHaptics Engine.
7. [**Kinetic Effects**](./docs/wiki/KineticEffects.md) - Discrete events and HSB modifiers.
8. [**Kinetic Gestures**](./docs/wiki/KineticGestures.md) - Touchless operation and IMU patterns (roadmap).
9. [**Profiles & Configuration**](./docs/wiki/Profiles.md) - SD card layout, `profile.json` schema, loading and switching.
10. [**Action Bus & Effects**](./docs/wiki/SaberAction.md) - Event processing, button gestures and trigger logic.
11. [**Diagnostics**](./docs/wiki/Diagnostics.md) - On-device real-time metrics and the CSV format.
12. [**sdkconfig Overrides**](./docs/wiki/sdkconfig_overrides.md) - Every changed build option and why.

## 🤝 Contributing and License

See [CONTRIBUTING.md](./CONTRIBUTING.md) and the [Code of Conduct](./CODE_OF_CONDUCT.md). Changes are listed in [CHANGELOG.md](./CHANGELOG.md). InertialSaber OS is licensed under the GPL-3.0-or-later ([LICENSE.txt](./LICENSE.txt)).

**Optional AI tooling.** `.claude/` and `CLAUDE.md` configure Claude Code for contributors who use it: an orchestrator protocol, specialised subagents (architecture, implementation, audit, hardware review, documentation), procedure skills and guardrail hooks. They are not needed to build, flash or contribute, and some files they mention exist only in the maintainer's checkout.

---

**Developed for enthusiasts, engineered for performance.**
*InertialSaber OS: Feel the physics.*
