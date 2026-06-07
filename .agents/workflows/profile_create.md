---
description: Create a dynamic lightsaber profile by validating raw WAV assets, generating profile.json, and preparing the SD card directory layout.
---

# Workflow: Profile Creation and Validation

This workflow guides the process of creating a new lightsaber profile for InertialSaber OS. It utilizes an automation script to scan, validate audio parameters against hardware requirements, calculate transition timings, and produce the required dynamic folder structure and `profile.json` configuration file.

---

## 1. Supported Naming Patterns

Place your raw sound files inside a single source folder. The script scans and maps them according to these patterns:

*   **Hum Loop**: `hum.wav`
*   **Profile Selection Sound**: `font.wav`
*   **Low Swing**: `swingl*.wav`, `swing_l*.wav`, `lswing*.wav` (e.g. `swingl1.wav`, `swingl2.wav` ...)
*   **High Swing**: `swingh*.wav`, `swing_h*.wav`, `hswing*.wav` (e.g. `swingh1.wav`, `swingh2.wav` ...)
*   **Inertial Bursts**: `swng*.wav`, `burst*.wav`
*   **Ignition (Power On)**: `in*.wav`, `ignition*.wav`
*   **Retraction (Power Off)**: `out*.wav`, `retract*.wav`
*   **Blaster Blocks**: `blst*.wav`, `blaster*.wav`
*   **Clashes**: `clsh*.wav`, `clash*.wav`
*   **Drags**: `drag*.wav`
*   **Drag Ends**: `enddrag*.wav`, `dragend*.wav`, `drag_end*.wav`

---

## 2. Audio Format Constraints

InertialSaber OS's C++ audio channel engine plays uncompressed PCM files directly from the SD card. The files **MUST** match these hardware constraints:
1.  **Format**: RIFF WAVE PCM (uncompressed).
2.  **Channels**: Exactly **1 (Mono)**. Stereo files are not supported by the hardware channels.
3.  **Bit Depth**: Exactly **16-bit**.
4.  **Sample Rate**: **44100 Hz** is strongly recommended to avoid speed mismatches.

---

## 3. Automation Process

Run the automated validation and creation script using `python3` from the root of the project:

```bash
python3 .agents/scripts/create_profile.py <src_dir> <dest_root> <profile_name> <blade_hue>
```

### Parameters:
*   `<src_dir>`: Path to the directory containing your raw WAV files.
*   `<dest_root>`: Target directory where profiles are built (e.g. the path to your mounted SD card's `profiles/` directory, or a local staging directory).
*   `<profile_name>`: Folder and profile ID to generate (e.g., `sith_red`, `luke_green`).
*   `<blade_hue>`: Integer Hue value (HSB color space, 0 to 359).
    *   *Examples*: Red = `0`, Green = `120`, Blue = `240`, Purple = `280`, Cyan = `180`.

### What the script does automatically:
1.  **WAV Header Validation**: Reads headers using the Python standard `wave` library to verify exact Mono + 16-bit PCM properties, protecting the microcontroller from runtime crashes.
2.  **Duration Calculations**: Reads the exact duration of `in*.wav` and `out*.wav` files, and automatically configures `ignition_duration_ms` and `retraction_duration_ms` using their average lengths.
3.  **Count Detections**: Counts valid assets per category to auto-generate the `font_counts` properties.
4.  **Directory Mapping**: Copies, sorts, and renames files to match the exact OS subdirectory layout (e.g., `/swingl/swingl1.wav`, `/swng/swng1.wav`).
5.  **Config Generation**: Outputs a clean, pre-populated `profile.json` under `profiles/<profile_name>/`.

---

## 4. Finalizing and Deployment

1.  **Verify local staging folder**: Inspect the generated folder under `<dest_root>/<profile_name>/`. It should contain the `profile.json` and the subfolders containing the renamed wav files.
2.  **SD Card Deployment**: Copy the `<profile_name>` folder to `/sdcard/profiles/` (so the path is `/sdcard/profiles/<profile_name>/profile.json`).
3.  **Modify Configurations (Optional)**: If you need to tweak swing thresholds, LED counts, or custom lighting cycles, open and edit `/sdcard/profiles/<profile_name>/profile.json`.
4.  **Run on Hardware**: Cycle profiles by long-pressing (1.5 seconds) the button while the blade is retracted.
