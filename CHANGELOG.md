# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0]

First public release of InertialSaber OS for the ESP32-S3.

### Added

- Physics-driven motion processing: kinetic energy, orientation and axis rotation computed from the IMU and delivered to effects through the SaberAction bus.
- Inertial overload accumulator with burst and cooldown.
- Power state machine (off, igniting, on, retracting) with synchronized audio and blade animations: single click to ignite, double click to retract.
- Kinetic effects: clash, blaster and drag, each with matching audio and blade overlay.
- Inertial swing audio: crossfaded swing sounds driven by motion, with polyphonic mixing.
- Audio streaming from the SD card, with the hum sound preloaded in PSRAM; the swing sound pair starts sample-aligned.
- Data-driven profiles discovered from the SD card, switchable at run time while the blade is retracted, with the active profile remembered across restarts.
- IMU start-up fallback so the saber is usable while the IMU gravity estimate converges.
- Optional on-device metrics (`CONFIG_SABER_METRICS`, off by default) written as CSV files to the SD card.
