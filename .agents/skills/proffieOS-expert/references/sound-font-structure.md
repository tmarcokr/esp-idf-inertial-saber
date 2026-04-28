# Sound Font Structure and Requirements - Technical Specification

## SD Card Organization

### Root Directory Structure

```
SD Card Root
├── font1/
│   ├── hum.wav
│   ├── swingl01.wav
│   ├── swingh01.wav
│   ├── clash01.wav
│   └── ...
├── font2/
│   ├── humm/
│   │   ├── 01.wav
│   │   └── 02.wav
│   ├── swingl/
│   │   ├── 01.wav
│   │   └── 02.wav
│   └── ...
└── presets.ini
```

### Font Directory Naming

**Flexible Naming:**
- Any directory name works: `/hero/`, `/sith/`, `/custom_font_01/`
- No specific naming convention required
- Referenced in preset configuration

**Preset Configuration:**
```cpp
Preset presets[] = {
    {"Font Name", "tracks/music.wav",
     StylePtr<...>(),  // Blade style
     "font1"},         // Font directory name
};
```

## File Naming Patterns

### Pattern Types

The system recognizes three organizational patterns:

#### Pattern 1: FLAT (root level)
```
/font/
  clash01.wav
  clash02.wav
  clash03.wav
  swing01.wav
  swing02.wav
```

#### Pattern 2: SUBDIRS (redundant)
```
/font/
  clash/
    clash01.wav
    clash02.wav
  swing/
    swing01.wav
    swing02.wav
```

#### Pattern 3: NONREDUNDANT_SUBDIRS (efficient)
```
/font/
  clash/
    01.wav
    02.wav
    03.wav
  swing/
    01.wav
    02.wav
```

**Recommendation:** Use NONREDUNDANT_SUBDIRS for cleaner organization

### Numbering Rules

**Valid Numbering:**
```
01.wav, 02.wav, 03.wav        ✓ Leading zeros
1.wav, 2.wav, 3.wav            ✓ No leading zeros
001.wav, 002.wav, 003.wav      ✓ Three digits
```

**Invalid Numbering:**
```
01.wav, 2.wav, 03.wav          ✗ Inconsistent zero padding
1.wav, 3.wav, 5.wav            ✗ Gaps in sequence
file_a.wav, file_b.wav         ✗ Non-numeric
```

**Requirements:**
- Start at any number (0, 1, or other)
- No gaps in sequence
- Consistent digit count if using leading zeros
- Sequential numbering only

### Sub-Files (Advanced)

**Directory Structure:**
```
/font/
  clash/
    001/
      000.wav
      001.wav
      002.wav
    002/
      000.wav
      001.wav
```

**Usage:**
- Multiple variations per main effect
- Sub-directory numbers: 3 digits starting from 000
- Files within: any numbering scheme
- Random selection from appropriate sub-directory

### Alternative Fonts

**Directory Structure:**
```
/font/
  hum.wav
  clash01.wav
  alt000/
    hum.wav
    clash01.wav
  alt001/
    hum.wav
    clash01.wav
```

**Requirements:**
- Format: `alt###/` (exactly 3 digits: 000-999)
- Must contain identical file structure as main font
- Allows runtime font switching
- All alternatives scanned during initialization

## SmoothSwing V2 Files

### Required Files

**Low Frequency Swings:**
```
Primary naming:
/font/swingl01.wav
/font/swingl02.wav
...or...
/font/swingl/01.wav
/font/swingl/02.wav

Fallback naming:
/font/lswing01.wav
/font/lswing02.wav
...or...
/font/lswing/01.wav
/font/lswing/02.wav
```

**High Frequency Swings:**
```
Primary naming:
/font/swingh01.wav
/font/swingh02.wav
...or...
/font/swingh/01.wav
/font/swingh/02.wav

Fallback naming:
/font/hswing01.wav
/font/hswing02.wav
...or...
/font/hswing/01.wav
/font/hswing/02.wav
```

**Critical Rule:** Number of low swing files MUST match number of high swing files

**Example Valid Pairs:**
```
swingl01.wav ↔ swingh01.wav  ✓
swingl02.wav ↔ swingh02.wav  ✓
swingl03.wav ↔ swingh03.wav  ✓
```

**Example Invalid:**
```
swingl01.wav ↔ swingh01.wav  ✓
swingl02.wav ↔ swingh02.wav  ✓
swingl03.wav ↔ (missing)     ✗ Mismatch!
```

### Optional Accent Files

**Accent Swings (Polyphonic):**
```
/font/swng01.wav
/font/swng02.wav
...or...
/font/swng/01.wav
/font/swng/02.wav
```

**Accent Swings (Monophonic):**
```
/font/swing01.wav
/font/swing02.wav
...or...
/font/swing/01.wav
/font/swing/02.wav
```

**Accent Slashes:**
```
/font/slsh01.wav
/font/slsh02.wav
...or...
/font/slsh/01.wav
/font/slsh/02.wav
```

**Activation:**
- Require `AccentSwingSpeedThreshold > 0` in config
- Triggered on fast swings (speed > threshold)
- Slashes triggered on high acceleration (> 260 deg/s²)

## Hum Sound Files

### Standard Hum

**Single File:**
```
/font/hum.wav
```
- Loops continuously
- Single variation

**Multiple Files:**
```
/font/hum01.wav
/font/hum02.wav
/font/hum03.wav
...or...
/font/hum/01.wav
/font/hum/02.wav
/font/hum/03.wav
```
- Randomly selects on each loop
- Adds variation to idle sound

### Polyphonic Hum

**Separate Hum File:**
```
/font/humm.wav
...or...
/font/humm01.wav
/font/humm02.wav
```

**Usage:**
- Indicates polyphonic font
- Plays continuously in dedicated channel
- Effects play independently

## Complete Effect List

### Essential Effects

| Effect | File Name | Description |
|--------|-----------|-------------|
| **Hum** | `hum` | Idle sound (mono fonts) |
| **Hum (poly)** | `humm` | Idle sound (polyphonic fonts) |
| **Power On** | `poweron` or `out` | Ignition sound |
| **Power Off** | `poweroff`, `pwroff`, or `in` | Retraction sound |
| **Clash** | `clash` or `clsh` | Impact sound |
| **Swing L** | `swingl` or `lswing` | Low frequency swing (looped) |
| **Swing H** | `swingh` or `hswing` | High frequency swing (looped) |

### Optional Effects

| Effect | File Name | Description |
|--------|-----------|-------------|
| **Accent Swing** | `swng` or `swing` | Fast swing sound |
| **Accent Slash** | `slsh` | Aggressive swing sound |
| **Blast** | `blast` or `blst` | Deflecting blaster bolt |
| **Force** | `force` | Force push/pull |
| **Lockup Begin** | `bgnlock` | Blade lock start |
| **Lockup** | `lock` | Blade lock loop |
| **Lockup End** | `endlock` | Blade lock end |
| **Drag** | `drag` | Blade dragging on ground |
| **Font** | `font` | Voice: "Font name" |
| **Boot** | `boot` | Power-up sound |
| **Volume Up** | `volup` | Volume increase beep |
| **Volume Down** | `voldown` | Volume decrease beep |
| **Battery Low** | `battery` | Low battery warning |

### Pre/Post Effects

| Effect | File Name | Description |
|--------|-----------|-------------|
| **Pre-On** | `preon` | Pre-ignition effect |
| **Post-Off** | `postoff` or `pstoff` | Post-retraction effect |

### Color Change Effects

| Effect | File Name | Description |
|--------|-----------|-------------|
| **CC Change** | `ccchange` | Color change confirm |
| **CC Begin** | `ccbegin` or `ccstart` | Color change mode start |
| **CC End** | `ccend` | Color change mode end |

## Looping Configuration

### Self-Looping Effects

**Defined via EFFECT2 macro:**
```cpp
EFFECT2(hum, hum)        // hum loops to hum
EFFECT2(lock, lock)      // lockup loops to lockup
EFFECT2(drag, drag)      // drag loops to drag
```

**Behavior:**
- Plays file to completion
- Randomly selects same effect again
- Continues until interrupted

### Effect Chains

**Following Effects:**
```cpp
EFFECT(preon)
EFFECT_FOLLOW(preon, poweron)  // preon → poweron

EFFECT(poweron)
EFFECT_FOLLOW(poweron, hum)    // poweron → hum

EFFECT(poweroff)
EFFECT_FOLLOW(poweroff, postoff)  // poweroff → postoff
```

**Behavior:**
- First effect plays to completion
- Automatically triggers next effect
- No gap between effects

## Audio File Specifications

### Required Format

**Container:** WAV (RIFF WAVE)

**Header Structure:**
```
Bytes 0-3:   'R' 'I' 'F' 'F'  (0x52494646)
Bytes 4-7:   File size - 8
Bytes 8-11:  'W' 'A' 'V' 'E'  (0x57415645)
Bytes 12-15: 'f' 'm' 't' ' '  (0x666D7420)
```

**Format Chunk:**
```
Offset | Size | Value | Description
-------|------|-------|-------------
0      | 2    | 1     | PCM format
2      | 2    | 1-2   | Channels (1=mono, 2=stereo)
4      | 4    | *     | Sample rate (44100, 22050, 11025)
8      | 4    | *     | Byte rate
12     | 2    | *     | Block align
14     | 2    | 8-16  | Bits per sample
```

**Data Chunk:**
```
Offset | Size | Value | Description
-------|------|-------|-------------
0      | 4    | 'data'| Chunk ID (0x64617461)
4      | 4    | *     | Data size in bytes
8      | *    | *     | Audio samples
```

### Recommended Specifications

**Optimal Quality:**
- **Sample Rate:** 44100 Hz
- **Bit Depth:** 16-bit
- **Channels:** Mono (1)
- **Format:** PCM uncompressed

**Acceptable Alternatives:**
- **Sample Rate:** 22050 Hz (auto-upsampled)
- **Bit Depth:** 8-bit (auto-converted)
- **Channels:** Stereo (auto-mixed to mono)

### Format Conversions

**Stereo to Mono:**
```
mono_sample = (left_sample + right_sample) / 2
```

**8-bit to 16-bit:**
```
sample_16bit = (sample_8bit << 8) - 32768
```

**Sample Rate Conversion:**
- 22050 Hz → 44100 Hz: 2× upsampling (Lanczos filter)
- 11025 Hz → 44100 Hz: 4× upsampling
- 88200 Hz → 44100 Hz: 2× downsampling

### Raw Audio Support

**No Header:**
- Extension: `.raw` or `.wav` without valid header
- Assumes: 44100 Hz, 16-bit, mono
- Reads samples directly as `int16_t`

**Use Case:** Pre-processed audio data

## Font Type Detection

### NEC Style (Polyphonic)

**Indicators:**
```
/font/out01.wav    (ignition)
/font/in01.wav     (retraction)
```

**Characteristics:**
- Multiple sounds play simultaneously
- Each effect independent channel
- No crossfading
- Modern standard

**Swing Files:** `swng` (polyphonic accent swings)

### Plecter Style (Monophonic/Hybrid)

**Indicators:**
```
/font/poweron.wav
/font/poweroff.wav
```

**Without `humm` files:**
- **Monophonic:** One sound at a time
- Crossfading between effects
- Legacy style

**With `humm` files:**
- **Plecter Polyphonic:** Hum continuous, effects overlap
- Hybrid approach
- Balance of old and new

**Swing Files:** `swing` (monophonic, crossfaded)

### Auto-Detection Logic

```pseudocode
FUNCTION detect_font_type():
    has_out = file_exists("out")
    has_in = file_exists("in")
    has_poweron = file_exists("poweron")
    has_poweroff = file_exists("poweroff")
    has_humm = file_exists("humm")

    IF has_out OR has_in:
        RETURN NEC_POLYPHONIC
    ELSE IF has_poweron OR has_poweroff:
        IF has_humm:
            RETURN PLECTER_POLYPHONIC
        ELSE:
            RETURN MONOPHONIC
        END IF
    END IF

    ERROR("Unknown font type")
END FUNCTION
```

## Configuration Files

### smoothsw.ini

**Location:** `/font/smoothsw.ini`

**Format:**
```ini
Version=2
SwingSensitivity=450.0
MaximumHumDucking=75.0
SwingSharpness=1.75
SwingStrengthThreshold=20.0
Transition1Degrees=45.0
Transition2Degrees=160.0
Low2HighSeparationDegrees=180.0
High2LowSeparationDegrees=180.0
MaxSwingVolume=3.0
AccentSwingSpeedThreshold=0.0
AccentSlashAccelerationThreshold=260.0
```

**Purpose:** Per-font SmoothSwing V2 configuration override

### config.ini

**Location:** `/font/config.ini`

**Format:**
```ini
ProffieOSSwingSpeedThreshold=250
ProffieOSMaxSwingVolume=2.0
ProffieOSSwingOverlap=0.5
ProffieOSSwingVolumeSharpness=0.5
```

**Purpose:** General font configuration

### CFX Compatibility

**Alternate Parameter Names:**
```ini
smooth_sens=450.0      # SwingSensitivity
smooth_dampen=75.0     # MaximumHumDucking
smooth_sharp=1.75      # SwingSharpness
smooth_gate=20.0       # SwingStrengthThreshold
smooth_width1=45.0     # Transition1Degrees
smooth_width2=160.0    # Transition2Degrees
```

**Compatibility Mode:** ProffieOS reads both naming conventions

## Font Scanning Process

### Directory Scan Algorithm

```pseudocode
FUNCTION ScanFont(directory):
    effects = initialize_all_effects()

    FOR EACH file IN directory (recursive):
        filename = extract_filename(file)

        FOR EACH effect IN effects:
            IF effect.matches(filename):
                effect.add_file(file)
            END IF
        END FOR
    END FOR

    // Validate
    FOR EACH effect IN effects:
        effect.validate()  // Check for gaps, warn if issues
    END FOR
END FUNCTION
```

### Effect Matching

**Filename Parsing:**
```cpp
// Pattern: name + optional digits + ".wav"
regex: "^(name)(\\d+)?\\.wav$"

Examples:
"clash01.wav"  → name="clash", number=1
"hum.wav"      → name="hum", number=none
"swng17.wav"   → name="swng", number=17
```

**Validation:**
```pseudocode
FUNCTION validate_effect():
    IF file_count == 0:
        RETURN  // Optional effect, not present

    // Check for gaps
    expected_count = max_number - min_number + 1
    IF file_count < expected_count:
        WARNING("Missing files in effect")
    END IF

    // Check SmoothSwing pairs
    IF effect_name IN ["swingl", "swingh"]:
        partner = get_partner_effect()
        IF file_count != partner.file_count:
            ERROR("Swing L/H file count mismatch")
        END IF
    END IF
END FUNCTION
```

## Minimal SmoothSwing V2 Font

**Absolute Minimum Files:**
```
/font/
  hum.wav              # Idle sound
  swingl01.wav         # Low swing
  swingh01.wav         # High swing
  out.wav              # Ignition
  in.wav               # Retraction
  clash01.wav          # Impact
```

**Result:** Functional but basic lightsaber

**Recommended Minimum:**
```
/font/
  hum.wav              # Or hum01-03.wav for variation
  swingl01.wav
  swingl02.wav
  swingl03.wav
  swingh01.wav
  swingh02.wav
  swingh03.wav
  swng01.wav           # Accent swings
  swng02.wav
  slsh01.wav           # Accent slashes
  out01.wav
  out02.wav
  in01.wav
  in02.wav
  clash01-08.wav
  blast01-04.wav
  lock01.wav
  drag01.wav
  font.wav             # Voice: font name
```

**Result:** Full-featured lightsaber with variation

## Sound Design Guidelines for SmoothSwing

### Low vs High Swing Files

**Low Frequency (`swingl`):**
- Slower blade movement sounds
- Deeper pitch
- Less intense
- Used for: gentle swings, slower movements
- Duration: 1-3 seconds typical

**High Frequency (`swingh`):**
- Faster blade movement sounds
- Higher pitch
- More intense/aggressive
- Used for: fast swings, combat movements
- Duration: 1-3 seconds typical

**Design Tip:** Should feel like natural progression (low → high)

### Loop Points

**Critical for SmoothSwing:**
- Files MUST loop seamlessly
- No click/pop at loop point
- Use crossfade at loop boundary
- Test: play on infinite loop, listen for artifacts

**Tools:** Audacity, Adobe Audition (crossfade loop)

### Stereo vs Mono

**Recommendation:** Mono
- Smaller file size (50% reduction)
- No quality loss (output is mono anyway)
- Faster SD card reads

**If Stereo:** Will be automatically mixed to mono

### Volume Normalization

**Peak Normalization:**
- Normalize each effect to -0.1 dB
- Prevents clipping
- Maintains relative loudness

**RMS Normalization (Advanced):**
- Normalize hum to -18 dB RMS
- Normalize swings to -12 dB RMS
- Normalize clash to -6 dB RMS
- Creates consistent perceived loudness

### File Size Considerations

**SD Card Space:**
- 44100 Hz, 16-bit, mono ≈ 86 KB/second
- 3-second swing file ≈ 258 KB
- Full font (50 files) ≈ 10-20 MB typical

**Read Performance:**
- Class 10 SD card minimum
- UHS-I/UHS-II recommended
- Sequential reads preferred (defragment!)

## Common Issues

### Issue: "swingl and swingh file count mismatch"

**Cause:** Different number of low vs high swing files

**Solution:**
```bash
# Check counts
ls /font/swingl*.wav | wc -l
ls /font/swingh*.wav | wc -l

# Must match!
```

### Issue: Gaps in numbering

**Cause:** Missing file in sequence (e.g., 01, 02, 04 - missing 03)

**Solution:** Renumber files sequentially or add missing file

### Issue: Files not detected

**Cause:** Incorrect naming or location

**Check:**
- File extension is `.wav` (lowercase)
- No extra characters in filename
- Located in correct font directory
- Not in ignored subdirectory

### Issue: Clicks/pops during playback

**Causes:**
- Bad loop point in audio file
- SD card too slow
- Fragmented files

**Solutions:**
- Fix loop points (crossfade)
- Use faster SD card
- Defragment files
- Reduce audio quality (22050 Hz)

### Issue: SmoothSwing doesn't work

**Check:**
1. Both `swingl` and `swingh` files present
2. File counts match
3. Files actually loop (not one-shots)
4. SwingStrengthThreshold not too high
5. Motion sensor working

## Summary

### SmoothSwing V2 Requirements
- ✓ Matched pairs of `swingl`/`swingh` files
- ✓ Seamlessly looping audio
- ✓ 44100 Hz, 16-bit, mono WAV (recommended)
- ✓ Optional `swng`/`slsh` for accent sounds

### Organization Best Practices
- ✓ Use NONREDUNDANT_SUBDIRS pattern
- ✓ Sequential numbering with no gaps
- ✓ Include `smoothsw.ini` for custom tuning
- ✓ Normalize volumes consistently

### File Quality Guidelines
- ✓ Mono audio (smaller, faster)
- ✓ Peak normalize to -0.1 dB
- ✓ Perfect loop points
- ✓ Test on actual hardware before deploying

This structure provides the foundation for responsive, high-quality SmoothSwing V2 lightsaber sound fonts.
