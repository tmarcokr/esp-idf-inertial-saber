#!/usr/bin/env python3
import os
import sys
import shutil
import json
import wave
import re

def print_err(msg):
    print(f"\033[91m[ERROR] {msg}\033[0m", file=sys.stderr)

def print_warn(msg):
    print(f"\033[93m[WARNING] {msg}\033[0m")

def print_ok(msg):
    print(f"\033[92m[SUCCESS] {msg}\033[0m")

def print_info(msg):
    print(f"\033[94m[INFO] {msg}\033[0m")

def validate_wav(file_path):
    """
    Validates that a WAV file matches ESP32 AudioChannel constraints:
    - Must be a valid RIFF WAVE PCM file.
    - Must be Mono (1 channel).
    - Must be 16-bit depth (2 bytes per sample).
    Returns (duration_ms, sample_rate) if valid, or None if invalid.
    """
    try:
        with wave.open(file_path, 'rb') as w:
            n_channels = w.getnchannels()
            samp_width = w.getsampwidth()
            framerate = w.getframerate()
            n_frames = w.getnframes()
            
            # Format validation
            if n_channels != 1:
                print_err(f"File {os.path.basename(file_path)} has {n_channels} channels. ONLY Mono WAVs are supported.")
                return None
            if samp_width != 2:
                print_err(f"File {os.path.basename(file_path)} has {samp_width*8}-bit sample depth. ONLY 16-bit WAVs are supported.")
                return None
            if framerate != 44100:
                print_warn(f"File {os.path.basename(file_path)} has sample rate {framerate}Hz. Recommended is 44100Hz.")
                
            duration_ms = int((n_frames / framerate) * 1000) if framerate > 0 else 0
            return duration_ms, framerate
    except Exception as e:
        print_err(f"Failed to parse WAV header for {file_path}: {e}")
        return None

def build_profile(src_dir, dest_root, profile_name, blade_hue):
    if not os.path.isdir(src_dir):
        print_err(f"Source directory '{src_dir}' does not exist.")
        return False
        
    print_info(f"Scanning directory: {src_dir}")
    
    # Sound categories mapping: (pattern, target_subfolder, target_prefix)
    categories = {
        "hum": (r"^hum\.wav$", "", "hum"),
        "swingl": (r"^(swingl|swing_l|lswing)\d*\.wav$", "swingl", "swingl"),
        "swingh": (r"^(swingh|swing_h|hswing)\d*\.wav$", "swingh", "swingh"),
        "burst": (r"^(swng|burst)\d*\.wav$", "swng", "swng"),
        "in": (r"^(in|ignition)\d*\.wav$", "in", "in"),
        "out": (r"^(out|retract)\d*\.wav$", "out", "out"),
        "blaster": (r"^(blst|blaster)\d*\.wav$", "blst", "blst"),
        "clash": (r"^(clsh|clash)\d*\.wav$", "clsh", "clsh"),
        "drag": (r"^drag\d*\.wav$", "drag", "drag"),
        "drag_end": (r"^(enddrag|dragend|drag_end)\d*\.wav$", "enddrag", "enddrag"),
    }
    
    found_files = {cat: [] for cat in categories}
    
    for filename in os.listdir(src_dir):
        file_path = os.path.join(src_dir, filename)
        if not os.path.isfile(file_path) or not filename.lower().endswith(".wav"):
            continue
            
        lower_name = filename.lower()
        matched = False
        for cat, (pattern, _, _) in categories.items():
            if re.match(pattern, lower_name):
                found_files[cat].append(file_path)
                matched = True
                break
                
        if not matched:
            print_warn(f"Ignored unknown WAV file: {filename}")

    # Validation and statistics collection
    stats = {}
    valid_files = {cat: [] for cat in categories}
    durations = {cat: [] for cat in categories}
    
    # Sort files to ensure stable indexing
    for cat in found_files:
        found_files[cat].sort()

    print_info("Validating WAV assets format...")
    all_valid = True
    for cat, paths in found_files.items():
        for path in paths:
            val = validate_wav(path)
            if val is None:
                all_valid = False
            else:
                duration, rate = val
                valid_files[cat].append(path)
                durations[cat].append(duration)
                
    if not all_valid:
        print_err("WAV validation failures detected. Correct the audio format parameters before continuing.")
        return False

    if not valid_files["hum"]:
        print_warn("No hum.wav file found. Hum is critical for standard loops.")

    # Calculate count metadata
    # swing_pair matches the minimum count between swingl and swingh
    swing_l_count = len(valid_files["swingl"])
    swing_h_count = len(valid_files["swingh"])
    swing_pair_count = min(swing_l_count, swing_h_count)
    if swing_l_count != swing_h_count:
        print_warn(f"Mismatched swing counts: swingl={swing_l_count}, swingh={swing_h_count}. Pairing count set to {swing_pair_count}.")

    # Calculate average timings (ms)
    avg_ignite_ms = int(sum(durations["in"]) / len(durations["in"])) if valid_files["in"] else 800
    avg_retract_ms = int(sum(durations["out"]) / len(durations["out"])) if valid_files["out"] else 500
    
    # Construct target directories
    profile_dest = os.path.join(dest_root, profile_name)
    if os.path.exists(profile_dest):
        print_warn(f"Destination '{profile_dest}' already exists. Overwriting content.")
        shutil.rmtree(profile_dest)
    os.makedirs(profile_dest, exist_ok=True)
    
    print_info(f"Copying and renaming assets to: {profile_dest}")
    
    # Copy & rename files to match InertialSaber OS expectations
    for cat, (_, subfolder, prefix) in categories.items():
        if subfolder:
            os.makedirs(os.path.join(profile_dest, subfolder), exist_ok=True)
            
        paths = valid_files[cat]
        # For swing pairs, truncate to the minimum paired amount
        if cat in ["swingl", "swingh"]:
            paths = paths[:swing_pair_count]
            
        for idx, src_path in enumerate(paths):
            if cat == "hum":
                dest_name = "hum.wav"
            else:
                dest_name = f"{prefix}{idx+1}.wav"
                
            dest_path = os.path.join(profile_dest, subfolder, dest_name)
            shutil.copy2(src_path, dest_path)

    # Generate profile.json structure
    profile_data = {
        "name": profile_name,
        "root_path": f"profiles/{profile_name}/",
        "overload": {
            "threshold_g": 1.0,
            "charge_rate": 2.0,
            "drain_rate": 0.5,
            "burst_cooldown_ms": 1500.0
        },
        "swing": {
            "idle_threshold_g": 0.15,
            "max_threshold_g": 1.0,
            "crossfade_low_g": 0.4,
            "crossfade_high_g": 1.0,
            "gravity_influence": 0.2,
            "hum_base_volume": 8000,
            "hum_max_ducking": 0.75,
            "swap_cooldown_ms": 1000,
            "swap_min_volume": 0.40,
            "clash_threshold_g": 2.0
        },
        "font_counts": {
            "hum": 1 if valid_files["hum"] else 0,
            "swing_pair": swing_pair_count,
            "burst": len(valid_files["burst"]),
            "in": len(valid_files["in"]),
            "out": len(valid_files["out"]),
            "blaster": len(valid_files["blaster"]),
            "clash": len(valid_files["clash"]),
            "drag": len(valid_files["drag"]),
            "drag_end": len(valid_files["drag_end"])
        },
        "blade_timings": {
            "ignition_duration_ms": avg_ignite_ms,
            "retraction_duration_ms": avg_retract_ms,
            "blaster_duration_ms": 250,
            "clash_duration_ms": 150
        },
        "blade_leds": {
            "blaster_count": 3,
            "drag_count": 8
        },
        "light": {
            "blade_base_hue": blade_hue,
            "idle_base_freq": 1.0,
            "idle_pulse_depth": 0.15,
            "max_thermal_bleed": 0.80,
            "flicker_intensity": 0.20,
            "burst_duration_ms": 150
        }
    }

    json_path = os.path.join(profile_dest, "profile.json")
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(profile_data, f, indent=4)
        
    print_ok(f"Created profile.json configuration file at: {json_path}")
    print_ok(f"Successfully generated profile '{profile_name}' folder at: {profile_dest}")
    print_info("Ready to copy to the SD card under the '/profiles/' path.")
    return True

if __name__ == "__main__":
    if len(sys.argv) < 5:
        print("Usage: python3 create_profile.py <src_dir> <dest_root> <profile_name> <blade_hue>")
        print("Example: python3 create_profile.py /path/to/raw_wavs /media/user/SDCARD/profiles sith_red 0")
        sys.exit(1)
        
    src = sys.argv[1]
    dest = sys.argv[2]
    name = sys.argv[3]
    try:
        hue = int(sys.argv[4])
    except ValueError:
        hue = 240
        
    success = build_profile(src, dest, name, hue)
    sys.exit(0 if success else 1)
