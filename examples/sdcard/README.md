# Example SD card layout

`profiles/example/profile.json` is a complete, valid profile. No audio files are shipped: add your own
44.1 kHz, mono, 16-bit PCM WAV files before copying `profiles/` to the root of the SD card.

The firmware reads `/sdcard/profiles/<name>/profile.json` and looks for the sound font under the profile's
`root_path`. `font_counts` in the JSON must match the number of files in each folder, numbered from 1
(`swingl1.wav`, `swingl2.wav`, ...).

```text
profiles/example/
    profile.json
    hum.wav            looping hum
    font.wav           played when the profile is selected
    swingl/            swingl1.wav ...  low swing (paired with swingh/)
    swingh/            swingh1.wav ...  high swing
    swng/              swng1.wav ...    inertial burst
    in/                in1.wav ...      ignition
    out/               out1.wav ...     retraction
    clsh/              clsh1.wav ...    clash
    blst/              blst1.wav ...    blaster block
    drag/              drag1.wav ...    drag loop
    enddrag/           enddrag1.wav ... drag release
```

`tools/create_profile.py` builds this layout and a matching `profile.json` from a folder of raw WAV files.
