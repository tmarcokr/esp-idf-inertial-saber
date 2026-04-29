# Components Architecture Reference

This document provides a strictly technical overview of the C++ components located in the `components/` directory. It focuses on software architecture, not domain logic.

## Component Map

### Audio Processing (`audio_engine`, `audio_channel`, `i2s_transmitter`)
- **Structure**: A pipeline for mixing and transmitting digital audio buffers.
- **audio_engine**: Contains `PolyphonicMixer`. Manages the concurrency and mixing of multiple data streams.
- **audio_channel**: Data structures representing individual audio streams.
- **i2s_transmitter**: Low-level hardware interface for DMA-based I2S transmission.

### Sensor Interfacing (`mpu6050`)
- **Structure**: I2C driver and DMP data extraction.
- **mpu6050**: Focuses on configuring the I2C bus, loading DMP firmware, and retrieving raw quaternion/accelerometer data safely. 

### Visual Rendering (`smart_led`, `rgb_led`)
- **Structure**: Buffer management and protocol transmission for LEDs.
- **smart_led**: Utilizes an `Engine` and `Canvas` architecture. It handles pixel buffer manipulation and dispatches generic visual effects.
- **rgb_led**: Basic GPIO abstraction for RGB control.

### Basic I/O (`gpio_button`, `sd_card`)
- **Structure**: Hardware abstraction layers.
- **gpio_button**: Manages hardware interrupts and software debouncing logic.
- **sd_card**: Abstracts SPI/SDIO initialization and FAT filesystem mounting.

## Structural Integration Rules
1. **Dependency Injection**: Components should not tightly couple to each other unless explicitly designed to do so (e.g., `audio_channel` into `audio_engine`). Pass interfaces or pointers.
2. **Resource Contention**: Be mindful of FreeRTOS tasks interacting with the same hardware peripherals (e.g., I2C bus shared between different sensors).
3. **Performance Boundaries**: Audio and LED processing happen in real-time constraints. Do not introduce blocking operations (like file I/O or long delays) within these critical paths.
