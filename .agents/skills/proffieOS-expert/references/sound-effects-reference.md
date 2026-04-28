# ProffieOS Sound Effects Reference

## Introduction

ProffieOS uses an `Effect` class to manage sound files for all audio effects. Each effect can have multiple numbered files (e.g., `clash01.wav`, `clash02.wav`), and the system will randomly select one when triggered. This document provides a comprehensive reference of all effects defined in the codebase.

### Effect System Overview

The Effect class tracks sound files by:
- **Filename prefix**: The base name (e.g., `clash`, `hum`, `swing`)
- **Numbering**: Files can be numbered (e.g., `clash01.wav`, `clash02.wav`) or unnumbered (`clash.wav`)
- **Sub-files**: Additional variants in subdirectories (e.g., `clash/001.wav`)
- **Alternatives**: Different versions in `alt000/`, `alt001/`, etc.

**Effect Macros**:
- `EFFECT(X)` - Defines a standard effect named X
- `EFFECT2(X, Y)` - Defines an effect X that loops/follows to effect Y

### Font Types

ProffieOS supports multiple sound font architectures:

**Monophonic Fonts**: Play one sound at a time. Common in older font packs. Use gapless transitions between sounds (e.g., poweron → hum).

**Polyphonic Fonts (Plecter)**: Play multiple sounds simultaneously. Use alternative file names:
- `humm.wav` instead of `hum.wav`
- `clsh.wav` instead of `clash.wav`
- `in.wav`/`out.wav` instead of `poweron.wav`/`poweroff.wav`

**NEC Fonts**: Polyphonic fonts that use `in.wav` and `out.wav` for ignition/retraction.

### File Organization Patterns

Effects can be organized in three ways:

1. **FLAT**: `effectname01.wav`, `effectname02.wav` (all in root directory)
2. **SUBDIRS**: `effectname/effectname01.wav`, `effectname/effectname02.wav`
3. **NONREDUNDANT_SUBDIRS**: `effectname/01.wav`, `effectname/02.wav` (efficient)

### Font Detection Logic

From `hybrid_font.h` (lines 151-173):

```cpp
// NEC font detection: has "in" or "out"
if (SFX_in || SFX_out) {
  monophonic_hum_ = false;
} else {
  monophonic_hum_ = SFX_poweron || SFX_poweroff || SFX_pwroff || SFX_blast;
}

// Plecter monophonic: has poweron/poweroff, no humm
if (monophonic_hum_) {
  if (SFX_clash || SFX_blaster || SFX_swing) {
    if (SFX_humm) {
      monophonic_hum_ = false;  // Plecter polyphonic
    } else {
      guess_monophonic_ = true;  // Monophonic
    }
  }
}
```

---

## Effect Categories

### 1. Core Sounds (Boot/Idle/Hum)

#### boot
**Source:** effect.h:739 or effect.h:742
**Files:** `boot.wav` (numbered variants: `boot01.wav`, `boot02.wav`, etc.)
**Type:** One-shot
**Trigger:** When the device powers on (before saber ignition)
**Fallback:** None
**Looping:** Follows to `bgnidle` (if ENABLE_IDLE_SOUND defined) or standalone
**Notes:** Can be played independently via EFFECT_BOOT. Used for startup sounds.

#### font
**Source:** effect.h:740 or effect.h:743
**Files:** `font.wav` (numbered variants: `font01.wav`, etc.)
**Type:** One-shot
**Trigger:** When a new font is loaded or during blade detect events (if bladein/bladeout missing)
**Fallback:** Beep at 1046.5 Hz if not found
**Looping:** Follows to `bgnidle` (if ENABLE_IDLE_SOUND defined) or standalone
**Notes:** Used for both monophonic and polyphonic fonts. Announces the font identity.

#### idle
**Source:** effect.h:737
**Files:** `idle.wav` (numbered variants: `idle01.wav`, etc.)
**Type:** Looped (EFFECT2)
**Trigger:** Continuously when blade is off and ENABLE_IDLE_SOUND is defined
**Fallback:** None
**Looping:** Loops to itself
**Notes:** Background ambient sound when saber is off. Only active if ENABLE_IDLE_SOUND is defined.

#### bgnidle
**Source:** effect.h:738
**Files:** `bgnidle.wav` (numbered variants: `bgnidle01.wav`, etc.)
**Type:** Looped (EFFECT2)
**Trigger:** Begins idle loop after boot/font sound completes
**Fallback:** Falls back to `idle`
**Looping:** Loops to `idle`
**Notes:** "Begin idle" - transition into idle loop. Only active if ENABLE_IDLE_SOUND is defined.

#### hum
**Source:** effect.h:747
**Files:** `hum.wav` (numbered variants: `hum01.wav`, etc.)
**Type:** Looped (EFFECT2)
**Trigger:** Continuously while saber is ignited (monophonic fonts)
**Fallback:** None (required for monophonic fonts)
**Looping:** Loops to itself
**Notes:** Core sound for monophonic fonts. Gaplessly loops while blade is on.

#### humm
**Source:** effect.h:748
**Files:** `humm.wav` (numbered variants: `humm01.wav`, etc.)
**Type:** Looped (EFFECT2)
**Trigger:** Continuously while saber is ignited (polyphonic fonts)
**Fallback:** Falls back to `hum` if not found
**Looping:** Loops to itself
**Notes:** Polyphonic version of hum. Two 'm's indicate polyphonic. Presence of this file indicates Plecter polyphonic font.

---

### 2. Ignition/Retraction Sounds

#### poweron
**Source:** effect.h:750
**Files:** `poweron.wav` (numbered variants: `poweron01.wav`, etc.)
**Type:** One-shot
**Trigger:** Saber ignition (monophonic fonts)
**Fallback:** None
**Looping:** Transitions gaplessly to `hum` in monophonic mode
**Notes:** Presence indicates monophonic font. NEC/polyphonic fonts use `out` instead.

#### poweronf
**Source:** effect.h:761
**Files:** `poweronf.wav` (numbered variants: `poweronf01.wav`, etc.)
**Type:** One-shot
**Trigger:** Force power-on effect (specific gesture or mode)
**Fallback:** None
**Notes:** "Force poweron" - special ignition variant.

#### poweroff
**Source:** effect.h:751
**Files:** `poweroff.wav` (numbered variants: `poweroff01.wav`, etc.)
**Type:** One-shot
**Trigger:** Saber retraction (monophonic fonts)
**Fallback:** Fade out hum if not found
**Looping:** Follows to `pstoff` (postoff) if present
**Notes:** Monophonic retraction sound.

#### pwroff
**Source:** effect.h:752
**Files:** `pwroff.wav` (numbered variants: `pwroff01.wav`, etc.)
**Type:** One-shot
**Trigger:** Saber retraction (alternative to poweroff)
**Fallback:** None
**Looping:** Follows to `pstoff` (postoff)
**Notes:** Alternative naming for poweroff. System randomly picks between poweroff and pwroff if both exist.

#### out
**Source:** effect.h:769
**Files:** `out.wav` (numbered variants: `out01.wav`, etc.)
**Type:** One-shot
**Trigger:** Saber ignition (NEC/polyphonic fonts)
**Fallback:** Falls back to `poweron` if not found
**Notes:** Presence indicates NEC or polyphonic font. Polyphonic ignition sound.

#### fastout
**Source:** effect.h:770
**Files:** `fastout.wav` (numbered variants: `fastout01.wav`, etc.)
**Type:** One-shot
**Trigger:** Fast ignition (when turning on without preon)
**Fallback:** Falls back to `out` if not found
**Notes:** Alternative ignition sound for quick activation.

#### in
**Source:** effect.h:768
**Files:** `in.wav` (numbered variants: `in01.wav`, etc.)
**Type:** One-shot
**Trigger:** Saber retraction (NEC/polyphonic fonts)
**Fallback:** Falls back to `poweroff` if not found
**Looping:** Follows to `pstoff` (postoff)
**Notes:** Presence indicates NEC or polyphonic font. Polyphonic retraction sound.

#### preon
**Source:** effect.h:730
**Files:** `preon.wav` (numbered variants: `preon01.wav`, etc.)
**Type:** One-shot
**Trigger:** Pre-ignition sequence (plays before blade lights)
**Fallback:** None
**Looping:** Follows to `out` (or `poweron`)
**Notes:** Optional dramatic pre-ignition sound/effect. System waits for preon to complete before turning blade on.

#### pstoff
**Source:** effect.h:731
**Files:** `pstoff.wav` (numbered variants: `pstoff01.wav`, etc.)
**Type:** One-shot
**Trigger:** Post-retraction (after blade is off)
**Fallback:** None
**Looping:** Follows to `bgnidle` (if ENABLE_IDLE_SOUND defined)
**Notes:** Post-off sound that plays after retraction completes.

---

### 3. Impact Sounds

#### clash
**Source:** effect.h:753
**Files:** `clash.wav` (numbered variants: `clash01.wav`, etc.)
**Type:** One-shot
**Trigger:** Blade clash detected (monophonic fonts)
**Fallback:** None
**Notes:** Monophonic clash sound. Interrupts hum briefly then returns to hum. Also used as fallback for stab and endlock.

#### clsh
**Source:** effect.h:767
**Files:** `clsh.wav` (numbered variants: `clsh01.wav`, etc.)
**Type:** One-shot
**Trigger:** Blade clash detected (polyphonic fonts)
**Fallback:** Falls back to `clash`
**Notes:** Polyphonic clash sound. Plays over hum without interrupting.

#### stab
**Source:** effect.h:755
**Files:** `stab.wav` (numbered variants: `stab01.wav`, etc.)
**Type:** One-shot
**Trigger:** Stab gesture/thrust detected
**Fallback:** Falls back to `clash` if not found
**Notes:** Works with both monophonic and polyphonic fonts.

#### blast
**Source:** effect.h:823
**Files:** `blast.wav` (numbered variants: `blast01.wav`, etc.)
**Type:** One-shot
**Trigger:** Blaster mode firing (prop-specific)
**Fallback:** None
**Notes:** Not to be confused with `blst` (saber blaster deflection) or `blaster` (saber blocking).

#### blaster
**Source:** effect.h:759
**Files:** `blaster.wav` (numbered variants: `blaster01.wav`, etc.)
**Type:** One-shot
**Trigger:** Blaster deflection (monophonic fonts)
**Fallback:** None
**Notes:** Sound when saber deflects a blaster bolt.

#### blst
**Source:** effect.h:766
**Files:** `blst.wav` (numbered variants: `blst01.wav`, etc.)
**Type:** One-shot
**Trigger:** Blaster deflection (polyphonic fonts)
**Fallback:** Falls back to `blaster`
**Notes:** Polyphonic version of blaster deflection.

---

### 4. Sustained Effects (Lockup/Drag/Melt/Lightning Block)

These effects use a begin/loop/end pattern for continuous effects.

#### lockup
**Source:** effect.h:760
**Files:** `lockup.wav` (numbered variants: `lockup01.wav`, etc.)
**Type:** Looped (EFFECT2)
**Trigger:** Lockup mode active (blade-to-blade contact, monophonic)
**Fallback:** None
**Looping:** Loops to itself
**Notes:** Monophonic sustained lockup. Interrupts and replaces hum during lockup.

#### lock
**Source:** effect.h:771
**Files:** `lock.wav` (numbered variants: `lock01.wav`, etc.)
**Type:** Looped (EFFECT2)
**Trigger:** Lockup mode active (polyphonic fonts)
**Fallback:** Falls back to `lockup`
**Looping:** Loops to itself
**Notes:** Polyphonic sustained lockup. Plays over hum.

#### bgnlock
**Source:** effect.h:762
**Files:** `bgnlock.wav` (numbered variants: `bgnlock01.wav`, etc.)
**Type:** One-shot
**Trigger:** Beginning of lockup
**Fallback:** Uses main lockup sound if not found
**Notes:** Transition sound when entering lockup. Works with both monophonic and polyphonic. Falls back to `lock`/`lockup` if not present.

#### endlock
**Source:** effect.h:763
**Files:** `endlock.wav` (numbered variants: `endlock01.wav`, etc.)
**Type:** One-shot
**Trigger:** End of lockup
**Fallback:** Falls back to `clash`
**Notes:** Transition sound when exiting lockup. Plecter endlock support. Used for polyphonic fonts.

#### drag
**Source:** effect.h:785
**Files:** `drag.wav` (numbered variants: `drag01.wav`, etc.)
**Type:** Looped (EFFECT2)
**Trigger:** Drag mode (blade tip dragging on ground)
**Fallback:** None
**Looping:** Loops to itself
**Notes:** Replaces lockup sound in drag mode if present.

#### bgndrag
**Source:** effect.h:783
**Files:** `bgndrag.wav` (numbered variants: `bgndrag01.wav`, etc.)
**Type:** One-shot
**Trigger:** Beginning of drag
**Fallback:** Uses `bgnlock` if not found
**Notes:** Transition into drag mode.

#### enddrag
**Source:** effect.h:786
**Files:** `enddrag.wav` (numbered variants: `enddrag01.wav`, etc.)
**Type:** One-shot
**Trigger:** End of drag
**Fallback:** Falls back to `endlock`, then `clash`
**Notes:** Transition out of drag mode.

#### melt
**Source:** effect.h:789
**Files:** `melt.wav` (numbered variants: `melt01.wav`, etc.)
**Type:** Looped (EFFECT2)
**Trigger:** Melt mode (melting through door/wall)
**Fallback:** Falls back to `drag` if not found
**Looping:** Loops to itself
**Notes:** Sustained melting sound. Falls back to drag → lock → lockup chain.

#### bgnmelt
**Source:** effect.h:788
**Files:** `bgnmelt.wav` (numbered variants: `bgnmelt01.wav`, etc.)
**Type:** One-shot
**Trigger:** Beginning of melt
**Fallback:** Falls back to `bgndrag`, then `bgnlock`
**Notes:** Transition into melt mode.

#### endmelt
**Source:** effect.h:790
**Files:** `endmelt.wav` (numbered variants: `endmelt01.wav`, etc.)
**Type:** One-shot
**Trigger:** End of melt
**Fallback:** Falls back to `enddrag`, then `endlock`, then `clash`
**Notes:** Transition out of melt mode.

#### lb
**Source:** effect.h:795
**Files:** `lb.wav` (numbered variants: `lb01.wav`, etc.)
**Type:** Looped (EFFECT2)
**Trigger:** Lightning block mode (blocking Force lightning)
**Fallback:** Falls back to standard lockup chain
**Looping:** Loops to itself
**Notes:** Sustained lightning blocking sound.

#### bgnlb
**Source:** effect.h:794
**Files:** `bgnlb.wav` (numbered variants: `bgnlb01.wav`, etc.)
**Type:** One-shot
**Trigger:** Beginning of lightning block
**Fallback:** Falls back to `bgnlock`
**Notes:** Transition into lightning block mode.

#### endlb
**Source:** effect.h:796
**Files:** `endlb.wav` (numbered variants: `endlb01.wav`, etc.)
**Type:** One-shot
**Trigger:** End of lightning block
**Fallback:** Falls back to `endlock`, then `clash`
**Notes:** Transition out of lightning block mode.

---

### 5. Motion Sounds (Swing/Spin)

#### swing
**Source:** effect.h:749
**Files:** `swing.wav` (numbered variants: `swing01.wav`, etc.)
**Type:** One-shot
**Trigger:** Swing motion detected (monophonic fonts, or when swng not present)
**Fallback:** None
**Notes:** Basic swing sound. In monophonic mode, interrupts hum. In polyphonic, can play multiple simultaneously based on ProffieOSSwingOverlap config.

#### swng
**Source:** effect.h:772
**Files:** `swng.wav` (numbered variants: `swng01.wav`, etc.)
**Type:** One-shot
**Trigger:** Swing motion detected (polyphonic fonts)
**Fallback:** Falls back to `swing`
**Notes:** Polyphonic swing sound. Plays over hum without interrupting.

#### slsh
**Source:** effect.h:773
**Files:** `slsh.wav` (numbered variants: `slsh01.wav`, etc.)
**Type:** One-shot
**Trigger:** Aggressive swing (slash) - acceleration above ProffieOSSlashAccelerationThreshold
**Fallback:** Falls back to `swng`, then `swing`
**Notes:** More aggressive swing variant triggered by fast acceleration.

#### spin
**Source:** effect.h:757
**Files:** `spin.wav` (numbered variants: `spin01.wav`, etc.)
**Type:** One-shot
**Trigger:** Spin gesture detected (rotation > ProffieOSSpinDegrees, default 360°)
**Fallback:** None
**Notes:** Only active if ENABLE_SPINS is defined. Works with both monophonic and polyphonic fonts. Also used as fallback for armed mode hum in Thermal Detonator mode.

#### swingl
**Source:** effect.h:777
**Files:** `swingl.wav` (numbered variants: `swingl01.wav`, etc.)
**Type:** Looped (EFFECT2)
**Trigger:** SmoothSwing V1/V2 - low swing speed
**Fallback:** None
**Looping:** Loops to itself
**Notes:** Looped swing for SmoothSwing fonts. LOW speed variant.

#### swingh
**Source:** effect.h:778
**Files:** `swingh.wav` (numbered variants: `swingh01.wav`, etc.)
**Type:** Looped (EFFECT2)
**Trigger:** SmoothSwing V1/V2 - high swing speed
**Fallback:** None
**Looping:** Loops to itself
**Notes:** Looped swing for SmoothSwing fonts. HIGH speed variant.

#### lswing
**Source:** effect.h:779
**Files:** `lswing.wav` (numbered variants: `lswing01.wav`, etc.)
**Type:** Looped (EFFECT2)
**Trigger:** SmoothSwing - low swing speed (Plecter naming)
**Fallback:** None
**Looping:** Loops to itself
**Notes:** Plecter naming convention for low swing. Same as swingl.

#### hswing
**Source:** effect.h:780
**Files:** `hswing.wav` (numbered variants: `hswing01.wav`, etc.)
**Type:** Looped (EFFECT2)
**Trigger:** SmoothSwing - high swing speed (Plecter naming)
**Fallback:** None
**Looping:** Loops to itself
**Notes:** Plecter naming convention for high swing. Same as swingh.

---

### 6. Special Effects (Force/Quote/BladeDetect)

#### force
**Source:** effect.h:754
**Files:** `force.wav` (numbered variants: `force01.wav`, etc.)
**Type:** One-shot
**Trigger:** Force effect gesture (user-defined)
**Fallback:** None
**Notes:** Generic Force power sound. Works with both monophonic and polyphonic fonts.

#### quote
**Source:** effect.h:774
**Files:** `quote.wav` (numbered variants: `quote01.wav`, etc.)
**Type:** One-shot
**Trigger:** Quote playback (user-triggered)
**Fallback:** None
**Notes:** Random quote/dialog from character. Polyphonic playback.

#### bladein
**Source:** effect.h:745
**Files:** `bladein.wav` (numbered variants: `bladein01.wav`, etc.)
**Type:** One-shot
**Trigger:** Blade inserted/detected
**Fallback:** Falls back to `font`, then beep
**Notes:** Blade detection sound when blade is connected. Works with both monophonic and polyphonic.

#### bladeout
**Source:** effect.h:746
**Files:** `bladeout.wav` (numbered variants: `bladeout01.wav`, etc.)
**Type:** One-shot
**Trigger:** Blade removed/disconnected
**Fallback:** Falls back to `font`, then beep
**Notes:** Blade detection sound when blade is disconnected. Works with both monophonic and polyphonic.

---

### 7. UI/System Sounds (Color Change/Battery/Menu)

#### color
**Source:** effect.h:805
**Files:** `color.wav` (numbered variants: `color01.wav`, etc.)
**Type:** One-shot
**Trigger:** Color change mode activated (when ccbegin not present)
**Fallback:** Beep sequence (1000 Hz, 1414 Hz, 2000 Hz)
**Notes:** Generic color change sound.

#### ccbegin
**Source:** effect.h:806
**Files:** `ccbegin.wav` (numbered variants: `ccbegin01.wav`, etc.)
**Type:** One-shot
**Trigger:** Entering color change mode
**Fallback:** Falls back to `color`, then beeps
**Notes:** Color change begin sound.

#### ccend
**Source:** effect.h:807
**Files:** `ccend.wav` (numbered variants: `ccend01.wav`, etc.)
**Type:** One-shot
**Trigger:** Exiting color change mode
**Fallback:** Beep sequence (2000 Hz, 1414 Hz, 1000 Hz - reverse of ccbegin)
**Notes:** Color change end sound.

#### ccchange
**Source:** effect.h:808
**Files:** `ccchange.wav` (numbered variants: `ccchange01.wav`, etc.)
**Type:** One-shot
**Trigger:** Changing to next/previous color in color change mode
**Fallback:** Beep at 2000 Hz
**Notes:** Sound when cycling through colors.

#### altchng
**Source:** effect.h:810
**Files:** `altchng.wav` (numbered variants: `altchng01.wav`, etc.)
**Type:** One-shot
**Trigger:** Switching to alternative sound font (alt000, alt001, etc.)
**Fallback:** None
**Notes:** Played when cycling through font alternatives.

#### chhum
**Source:** effect.h:811
**Files:** `chhum.wav` (numbered variants: `chhum01.wav`, etc.)
**Type:** One-shot
**Trigger:** Transitional hum when switching alternatives
**Fallback:** Falls back to regular hum
**Notes:** Smooth transition when changing font alternatives while blade is on.

#### mclick
**Source:** effect.h:814
**Files:** `mclick.wav` (numbered variants: `mclick01.wav`, etc.)
**Type:** One-shot
**Trigger:** Menu navigation click
**Fallback:** Beep at 2000 Hz
**Notes:** UI feedback for menu system.

#### lowbatt
**Source:** effect.h:825
**Files:** `lowbatt.wav` (numbered variants: `lowbatt01.wav`, etc.)
**Type:** One-shot
**Trigger:** Low battery warning
**Fallback:** None
**Notes:** Plays when battery voltage drops below threshold.

---

### 8. Thermal Detonator Mode

#### armhum
**Source:** effect.h:800
**Files:** `armhum.wav` (numbered variants: `armhum01.wav`, etc.)
**Type:** Looped (EFFECT2)
**Trigger:** Thermal detonator armed state (continuous)
**Fallback:** Falls back to `swing` (Thermal-D fallback)
**Looping:** Loops to itself
**Notes:** Armed state hum for thermal detonator prop mode.

#### bgnarm
**Source:** effect.h:799
**Files:** `bgnarm.wav` (numbered variants: `bgnarm01.wav`, etc.)
**Type:** One-shot
**Trigger:** Beginning of thermal detonator arm sequence
**Fallback:** Uses `armhum` if not found
**Notes:** Transition into armed state.

#### endarm
**Source:** effect.h:801
**Files:** `endarm.wav` (numbered variants: `endarm01.wav`, etc.)
**Type:** One-shot
**Trigger:** End of armed state (disarm)
**Fallback:** None
**Notes:** Transition out of armed state.

#### boom
**Source:** effect.h:802
**Files:** `boom.wav` (numbered variants: `boom01.wav`, etc.)
**Type:** One-shot
**Trigger:** Thermal detonator explosion
**Fallback:** Falls back to `clash` (Thermal-D fallback)
**Notes:** Explosion sound when detonator triggers.

---

### 9. Blaster Mode (Autofire)

#### auto
**Source:** effect.h:819
**Files:** `auto.wav` (numbered variants: `auto01.wav`, etc.)
**Type:** Looped (EFFECT2)
**Trigger:** Blaster autofire mode active
**Fallback:** None
**Looping:** Loops to itself
**Notes:** Sustained automatic fire sound. Prop-specific feature.

#### bgnauto
**Source:** effect.h:818
**Files:** `bgnauto.wav` (numbered variants: `bgnauto01.wav`, etc.)
**Type:** One-shot
**Trigger:** Beginning of autofire
**Fallback:** Uses `auto` if not found
**Notes:** Transition into autofire mode. Not found in fonts yet but supported by code.

#### endauto
**Source:** effect.h:820
**Files:** `endauto.wav` (numbered variants: `endauto01.wav`, etc.)
**Type:** One-shot
**Trigger:** End of autofire
**Fallback:** Falls back to `blast`
**Notes:** Transition out of autofire mode. Not found in fonts yet but supported by code.

---

## Effect Chains and Fallbacks

### Lockup Chain
1. **Begin**: `bgnlock` → (if not found) → `lock`/`lockup`
2. **Loop**: `lock` (polyphonic) or `lockup` (monophonic)
3. **End**: `endlock` → (if not found) → `clash`

### Drag Chain
1. **Begin**: `bgndrag` → (if not found) → `bgnlock` → `lock`/`lockup`
2. **Loop**: `drag` → (if not found) → `lock`/`lockup`
3. **End**: `enddrag` → (if not found) → `endlock` → `clash`

### Melt Chain
1. **Begin**: `bgnmelt` → (if not found) → `bgndrag` → `bgnlock` → `lock`/`lockup`
2. **Loop**: `melt` → (if not found) → `drag` → `lock`/`lockup`
3. **End**: `endmelt` → (if not found) → `enddrag` → `endlock` → `clash`

### Lightning Block Chain
1. **Begin**: `bgnlb` → (if not found) → `bgnlock` → `lock`/`lockup`
2. **Loop**: `lb` → (if not found) → `lock`/`lockup`
3. **End**: `endlb` → (if not found) → `endlock` → `clash`

### Ignition Chain (Monophonic)
`preon` → `poweron` → `hum` (looped)

### Ignition Chain (Polyphonic)
`preon` → `out`/`fastout` → `humm` (looped)

### Retraction Chain (Monophonic)
`hum` → `poweroff`/`pwroff` → `pstoff` → `bgnidle` (if enabled)

### Retraction Chain (Polyphonic)
`humm` → `in` → `pstoff` → `bgnidle` (if enabled)

### Idle Chain (ENABLE_IDLE_SOUND)
`boot` → `bgnidle` → `idle` (looped)

### Thermal Detonator Chain
`bgnarm` → `armhum` (looped) → `endarm` or `boom`

### Autofire Chain
`bgnauto` → `auto` (looped) → `endauto` → `blast`

---

## Quick Reference Table

| Effect Name | Line | File Pattern | Type | Loop | Polyphonic Alt | Primary Use |
|-------------|------|--------------|------|------|----------------|-------------|
| preon | 730 | preon.wav | M+P | No | - | Pre-ignition |
| pstoff | 731 | pstoff.wav | M+P | No | - | Post-retraction |
| idle | 737 | idle.wav | M+P | Yes | - | Background idle |
| bgnidle | 738 | bgnidle.wav | M+P | Yes | idle | Begin idle |
| boot | 739/742 | boot.wav | M+P | No | - | Power on |
| font | 740/743 | font.wav | M+P | No | - | Font ID |
| bladein | 745 | bladein.wav | M+P | No | - | Blade inserted |
| bladeout | 746 | bladeout.wav | M+P | No | - | Blade removed |
| hum | 747 | hum.wav | M | Yes | humm | Core hum (mono) |
| humm | 748 | humm.wav | P | Yes | - | Core hum (poly) |
| swing | 749 | swing.wav | M+P | No | swng | Basic swing |
| poweron | 750 | poweron.wav | M | No | out | Ignition (mono) |
| poweroff | 751 | poweroff.wav | M | No | in | Retraction (mono) |
| pwroff | 752 | pwroff.wav | M | No | in | Retraction alt |
| clash | 753 | clash.wav | M | No | clsh | Impact (mono) |
| force | 754 | force.wav | M+P | No | - | Force power |
| stab | 755 | stab.wav | M+P | No | - | Thrust |
| spin | 757 | spin.wav | M+P | No | - | Spin gesture |
| blaster | 759 | blaster.wav | M | No | blst | Deflection (mono) |
| lockup | 760 | lockup.wav | M | Yes | lock | Lockup (mono) |
| poweronf | 761 | poweronf.wav | M+P | No | - | Force ignition |
| bgnlock | 762 | bgnlock.wav | M+P | No | - | Begin lockup |
| endlock | 763 | endlock.wav | M+P | No | - | End lockup |
| blst | 766 | blst.wav | P | No | - | Deflection (poly) |
| clsh | 767 | clsh.wav | P | No | - | Impact (poly) |
| in | 768 | in.wav | P | No | - | Retraction (poly) |
| out | 769 | out.wav | P | No | - | Ignition (poly) |
| fastout | 770 | fastout.wav | P | No | - | Fast ignition |
| lock | 771 | lock.wav | P | Yes | - | Lockup (poly) |
| swng | 772 | swng.wav | P | No | - | Swing (poly) |
| slsh | 773 | slsh.wav | P | No | - | Slash |
| quote | 774 | quote.wav | P | No | - | Character quote |
| swingl | 777 | swingl.wav | M+P | Yes | lswing | SmoothSwing low |
| swingh | 778 | swingh.wav | M+P | Yes | hswing | SmoothSwing high |
| lswing | 779 | lswing.wav | M+P | Yes | - | SS low (Plecter) |
| hswing | 780 | hswing.wav | M+P | Yes | - | SS high (Plecter) |
| bgndrag | 783 | bgndrag.wav | M+P | No | - | Begin drag |
| drag | 785 | drag.wav | M+P | Yes | - | Drag loop |
| enddrag | 786 | enddrag.wav | M+P | No | - | End drag |
| bgnmelt | 788 | bgnmelt.wav | M+P | No | - | Begin melt |
| melt | 789 | melt.wav | M+P | Yes | - | Melt loop |
| endmelt | 790 | endmelt.wav | M+P | No | - | End melt |
| bgnlb | 794 | bgnlb.wav | M+P | No | - | Begin lightning |
| lb | 795 | lb.wav | M+P | Yes | - | Lightning loop |
| endlb | 796 | endlb.wav | M+P | No | - | End lightning |
| bgnarm | 799 | bgnarm.wav | M+P | No | - | Begin arm (det) |
| armhum | 800 | armhum.wav | M+P | Yes | - | Armed hum (det) |
| endarm | 801 | endarm.wav | M+P | No | - | Disarm (det) |
| boom | 802 | boom.wav | M+P | No | - | Explosion (det) |
| color | 805 | color.wav | P | No | - | Color change |
| ccbegin | 806 | ccbegin.wav | P | No | - | Begin CC |
| ccend | 807 | ccend.wav | P | No | - | End CC |
| ccchange | 808 | ccchange.wav | P | No | - | Change color |
| altchng | 810 | altchng.wav | M+P | No | - | Alt font change |
| chhum | 811 | chhum.wav | M+P | No | - | Change hum |
| mclick | 814 | mclick.wav | P | No | - | Menu click |
| bgnauto | 818 | bgnauto.wav | M+P | No | - | Begin autofire |
| auto | 819 | auto.wav | M+P | Yes | - | Autofire loop |
| endauto | 820 | endauto.wav | M+P | No | - | End autofire |
| blast | 823 | blast.wav | M+P | No | - | Blaster fire |
| lowbatt | 825 | lowbatt.wav | M+P | No | - | Low battery |

**Legend:**
- **M**: Monophonic font support
- **P**: Polyphonic font support
- **M+P**: Works with both
- **Loop**: Whether the effect loops to itself (EFFECT2)
- **Polyphonic Alt**: Alternative file name used in polyphonic fonts

---

## File Naming Conventions

### Numbered Files
Effects support numbered variants:
- `clash01.wav`, `clash02.wav`, `clash03.wav`
- System randomly selects one variant each time
- Can use leading zeros: `clash001.wav`, `clash002.wav`
- Leading zero count must be consistent across all files

### Unnumbered Files
Single file without number:
- `clash.wav` (works alongside or instead of numbered files)

### Sub-files Structure
Effects can have sub-variants:
- `clash/001.wav`, `clash/002.wav`, `clash/003.wav`
- Used for additional randomization within a numbered file selection
- Format: `effectname/001.wav` (must use 3 digits starting from 000)

### Alternative Fonts
Multiple font variations in same directory:
- `alt000/`, `alt001/`, `alt002/`
- Full font copies with different sound characteristics
- Switch between alternatives with EFFECT_ALT_SOUND
- Must use exactly 3 digits (alt000-alt999)

### Directory Structures

**FLAT:**
```
font_name/
  clash01.wav
  clash02.wav
  hum.wav
```

**SUBDIRS:**
```
font_name/
  clash/clash01.wav
  clash/clash02.wav
  hum/hum.wav
```

**NONREDUNDANT_SUBDIRS (efficient):**
```
font_name/
  clash/01.wav
  clash/02.wav
  hum.wav
```

**With alternatives:**
```
font_name/
  alt000/
    clash01.wav
    hum.wav
  alt001/
    clash01.wav
    hum.wav
```

---

## Configuration Variables (font_config.ini)

From `hybrid_font.h` (lines 8-120), fonts can include a `config.ini` file with these parameters:

### Volume Settings
- **humStart** (default: 100): Milliseconds before end of "out" to fade in hum
- **volHum** (default: 15): Hum volume (0-16 scale)
- **volEff** (default: 16): Effect volume (0-16 scale)
- **ProffieOSHumDelay** (default: -1): Milliseconds from beginning of out.wav to delay hum (-1 uses humStart)

### Swing Settings
- **ProffieOSSwingSpeedThreshold** (default: 250.0): Degrees/second to trigger swing
- **ProffieOSSwingVolumeSharpness** (default: 0.5): Response curve between swing speed and volume
- **ProffieOSMaxSwingVolume** (default: 2.0): Volume at swing speed threshold
- **ProffieOSSwingOverlap** (default: 0.5): Fraction of swing that must play before new swing (0=full overlap, 1=no overlap)
- **ProffieOSSmoothSwingDucking** (default: 0.2): Hum volume reduction during swing (20% reduction)
- **ProffieOSSwingLowerThreshold** (default: 200.0): Degrees/second below which swing stops
- **ProffieOSSlashAccelerationThreshold** (default: 130.0): Degrees/sec² to trigger slash instead of swing
- **ProffieOSMinSwingAcceleration** (default: 0.0): Min acceleration for file selection
- **ProffieOSMaxSwingAcceleration** (default: 0.0): Max acceleration for file selection (must be > min to enable)

### Spin Settings (ENABLE_SPINS)
- **ProffieOSSpinDegrees** (default: 360.0): Degrees blade must travel to trigger spin

### SmoothSwing Settings
- **ProffieOSSmoothSwingHumstart** (default: 0): Sync smoothswings with hum start (0=resume, 1=sync)

### Per-Effect Settings
Each effect can have individual settings in config.ini:
- **ProffieOS.SFX.effectname.paired** (default: false): Use same sound number as previous effect
- **ProffieOS.SFX.effectname.volume** (default: 100): Per-effect volume percentage

Example config.ini:
```ini
ProffieOSSwingSpeedThreshold=300
ProffieOSMaxSwingVolume=2.5
volHum=14
ProffieOS.SFX.clash.volume=120
ProffieOS.SFX.swing.paired=1
```

---

## Error Sounds

ProffieOS includes error message sounds in the root directory (not in fonts):

- **e_fnt_nf.wav**: Font directory not found (EFFECT_FONT_DIRECTORY_NOT_FOUND)
- **e_vp_nf.wav**: Voice pack not found (EFFECT_VOICE_PACK_NOT_FOUND)
- **e_blade.wav**: Error in blade array (EFFECT_ERROR_IN_BLADE_ARRAY)
- **e_in_fnt.wav**: Error in font directory (EFFECT_ERROR_IN_FONT_DIRECTORY)
- **e_vp_ver.wav**: Error in voice pack version (EFFECT_ERROR_IN_VOICE_PACK_VERSION)

---

## Notes

1. **Required Effects**: Most effects are optional. Only `hum` (or `humm`) is required for basic operation. Missing effects will fall back to alternatives or be skipped.

2. **Random Selection**: When multiple numbered files exist, the system randomly selects one. With `NO_REPEAT_RANDOM` defined, the system tries to avoid repeating the last played file.

3. **Paired Effects**: Effects can be "paired" (via config.ini) to use the same number as a previous effect (e.g., if `clash03.wav` played, and swing is paired, it will play `swing03.wav`).

4. **Effect Following**: The `EFFECT2` macro makes effects loop or chain to another effect. This is how `hum` continuously loops and how `preon` automatically triggers `poweron`.

5. **Font Scanning**: ProffieOS scans the SD card at startup and identifies all effects. Missing files in a sequence will generate warnings but won't prevent operation.

6. **File Extensions**: System supports `.wav`, `.raw`, and `.usl` audio files. All files for an effect must use the same extension.

7. **Sound Length Tracking**: The system tracks the length of currently playing effects for synchronization with blade effects (e.g., `WavLen<>` blade function).

8. **Alternative Fonts**: The `alt000/`, `alt001/` system allows multiple sound variations in the same physical font directory, selectable at runtime via EFFECT_ALT_SOUND.

---

## Implementation Details

### Effect Scanning (effect.h:209-276)
The `Scan()` method identifies files by:
1. Checking for `altNNN/` prefix
2. Matching effect name prefix
3. Detecting subdirectory structure
4. Parsing number (if present)
5. Identifying file extension
6. Counting files and sub-files

### Effect Selection (effect.h:436-462)
The `RandomFile()` method:
1. Checks if specific file selected via `Select()`
2. Falls back to `SaberBase::sound_number` if set
3. Otherwise randomly picks a file (avoiding repeats if NO_REPEAT_RANDOM defined)
4. Returns a `FileID` object containing effect, file number, sub-file ID, and alternative

### File Path Construction (effect.h:485-520)
The `GetName()` method builds the full path:
1. Starts with directory path
2. Adds `altNNN/` if found in alt directory
3. Adds effect name
4. Adds subdirectory structure based on pattern
5. Adds file number (with leading zeros)
6. Adds sub-file path (if applicable)
7. Adds file extension

---

## Additional Resources

- **ProffieOS Documentation**: https://pod.hubbe.net/
- **Sound Font Guide**: https://fredrik.hubbe.net/lightsaber/sound.html
- **Font Configuration**: https://pod.hubbe.net/config/sound-styles.html
- **Effect Testing**: Use the serial monitor to see which files are found during font scanning

---

*This reference is based on ProffieOS source code analysis of `sound/effect.h` (lines 1-899) and `sound/hybrid_font.h` (lines 1-935).*
