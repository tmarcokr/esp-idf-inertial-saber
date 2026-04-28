# Audio System Architecture - Technical Specification

## Overview

This document provides comprehensive technical details for implementing a lightsaber audio system with I2C or I2S DAC on ESP32 platforms, based on ProffieOS architecture.

## Audio Specifications

### Sample Rate
- **Primary Rate:** 44100 Hz (AUDIO_RATE constant)
- **Supported Rates:** 44100, 22050, 11025, 88200 Hz
- **Resampling:** Automatic upsampling/downsampling to match 44100 Hz output

### Bit Depth & Format
- **Native Format:** 16-bit signed integer (int16_t)
- **Range:** -32768 to +32767
- **Internal Processing:** Float for filters/mixers, converted to int16_t for output
- **WAV Support:** 8-bit and 16-bit PCM

### Channel Configuration
- **Output:** MONO (single channel)
- **Internal Processing:** Mono mixing
- **Stereo Compatibility:** Stereo WAV files averaged to mono

## Hardware Interface Options

### Option 1: I2S DAC (Recommended for ESP32)

**Advantages:**
- High sample rate support (44.1 kHz+)
- Low CPU overhead (DMA-driven)
- Accurate timing
- Common ESP32 peripheral

**Example DACs:**
- MAX98357A (I2S amplifier, no external components)
- PCM5102A (I2S DAC, high quality)
- UDA1334A (I2S DAC, affordable)

**ESP32 I2S Configuration:**
```cpp
i2s_config_t i2s_config = {
    .mode = I2S_MODE_MASTER | I2S_MODE_TX,
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,  // Mono
    .communication_format = I2S_COMM_FORMAT_I2S,
    .dma_buf_count = 2,
    .dma_buf_len = 512,  // Samples per buffer
    .use_apll = true,    // Use audio PLL for accurate clock
    .tx_desc_auto_clear = true
};
```

**Pin Configuration:**
```cpp
i2s_pin_config_t pin_config = {
    .bck_io_num = 26,    // Bit clock
    .ws_io_num = 25,     // Word select (LRCK)
    .data_out_num = 22,  // Data out
    .data_in_num = -1    // Not used
};
```

### Option 2: I2C DAC

**Challenges:**
- I2C bandwidth limitation (400 kHz = ~25 KB/s)
- Required: 44100 samples/s × 2 bytes = 88.2 KB/s
- **Solution:** DAC with internal FIFO buffer

**Recommended DACs:**
- WM8960 (I2C control, I2S data - hybrid approach)
- PCM5102A with I2C control (I2S for audio data)

**Note:** Pure I2C audio data transmission at 44.1 kHz is challenging. Hybrid approach (I2C control + I2S data) recommended.

### Option 3: Internal DAC (ESP32)

**ESP32 Built-in DAC:**
- 8-bit resolution (lower quality)
- 2 channels (GPIO25, GPIO26)
- Driven by I2S peripheral
- Adequate for prototyping

```cpp
dac_output_enable(DAC_CHANNEL_1);  // GPIO25
i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);
```

## DMA & Buffering Architecture

### Multi-Level Buffer System

```
┌─────────────────────────────────────┐
│ SD Card / Flash Storage (Sound Files) │
└──────────────┬──────────────────────┘
               │ 512-byte reads
               ▼
┌─────────────────────────────────────┐
│  File Read Buffer (512 bytes)       │
└──────────────┬──────────────────────┘
               │ WAV decoding
               ▼
┌─────────────────────────────────────┐
│ BufferedAudioStream (512 bytes × 7) │
│  - Per-player buffering              │
│  - Prevents underruns                │
└──────────────┬──────────────────────┘
               │ Mixing
               ▼
┌─────────────────────────────────────┐
│  Dynamic Mixer Output                │
└──────────────┬──────────────────────┘
               │ 44 samples
               ▼
┌─────────────────────────────────────┐
│  DMA Buffer (88 samples, double)     │
│  - Half-transfer interrupt           │
│  - Complete-transfer interrupt       │
└──────────────┬──────────────────────┘
               │ Hardware DMA
               ▼
┌─────────────────────────────────────┐
│     I2S/DAC Hardware                 │
└─────────────────────────────────────┘
```

### Buffer Sizes

| Level | Size | Purpose |
|-------|------|---------|
| File read | 512 bytes | SD card efficiency |
| Audio stream | 512 bytes | Per-player lookahead |
| DMA buffer | 88 samples (176 bytes) | Double-buffered output |

### DMA Buffer Details

**Configuration:**
```cpp
#define AUDIO_BUFFER_SIZE 44  // Samples
#define CHANNELS 1            // Mono
#define DMA_BUFFER_COUNT 2    // Double buffering

int16_t dac_dma_buffer[AUDIO_BUFFER_SIZE * 2 * CHANNELS];
```

**Total Size:** 88 samples = 176 bytes (mono)

**Interrupt Rate:**
```
interrupt_rate = sample_rate / buffer_size
                = 44100 Hz / 44
                = 1002.27 Hz
                ≈ 1 ms per interrupt
```

## Interrupt-Driven Audio

### ISR Structure

**Double Buffering:**
- DMA continuously transmits from buffer
- Half-transfer IRQ: Fill first half while DMA sends second half
- Complete-transfer IRQ: Fill second half while DMA sends first half

**Pseudocode:**
```cpp
volatile int current_buffer = 0;

void audio_isr() {
    // Determine which half to fill
    int16_t* fill_ptr;
    if (current_buffer == 0) {
        fill_ptr = &dac_dma_buffer[0];  // Fill first half
    } else {
        fill_ptr = &dac_dma_buffer[AUDIO_BUFFER_SIZE];  // Fill second half
    }

    // Request 44 samples from mixer
    dynamic_mixer.read(fill_ptr, AUDIO_BUFFER_SIZE);

    // Toggle buffer
    current_buffer = 1 - current_buffer;
}
```

### Timing Constraints

**Critical Requirement:** ISR must complete in < 1 ms

**CPU Budget (at 240 MHz ESP32):**
- 1 ms = 240,000 CPU cycles
- Mixer read: ~50,000 cycles (estimate)
- Headroom: ~190,000 cycles for other tasks

**Optimization Tips:**
- Use DMA for I2S transfer (zero CPU during transmission)
- Minimize ISR work (only fill buffer, no processing)
- Pre-calculate all volume/effects in background task

### ESP32 I2S DMA Configuration

```cpp
void setup_i2s_dma() {
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);

    // Enable DMA
    i2s_set_clk(I2S_NUM_0, 44100, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
}

void audio_task(void* param) {
    size_t bytes_written;
    int16_t buffer[AUDIO_BUFFER_SIZE];

    while (1) {
        // Fill buffer from mixer
        dynamic_mixer.read(buffer, AUDIO_BUFFER_SIZE);

        // Send to I2S (blocks until DMA ready)
        i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &bytes_written, portMAX_DELAY);
    }
}
```

**Note:** ESP32 I2S driver handles DMA automatically. Task-based approach simpler than ISR.

## WAV File Format Support

### Required Format

**Container:** RIFF WAVE
- **Magic Numbers:**
  - RIFF: 0x46464952
  - WAVE: 0x45564157
  - fmt: 0x20746D66
  - data: 0x61746164

**Format Chunk:**
```
Offset | Size | Description
-------|------|------------
0      | 2    | Format code (1 = PCM)
2      | 2    | Channels (1 or 2)
4      | 4    | Sample rate (44100, 22050, 11025)
8      | 4    | Byte rate
12     | 2    | Block align
14     | 2    | Bits per sample (8 or 16)
```

**Data Chunk:**
```
Offset | Size | Description
-------|------|------------
0      | 4    | 'data' (0x61746164)
4      | 4    | Data size in bytes
8      | N    | Audio samples
```

### Automatic Conversions

**Stereo to Mono:**
```cpp
int16_t stereo_to_mono(int16_t left, int16_t right) {
    return (left + right) / 2;
}
```

**8-bit to 16-bit:**
```cpp
int16_t convert_8bit(uint8_t sample) {
    return (sample << 8) - 32768;
}
```

**Sample Rate Conversion:**
- **2× Upsampling:** 22050 → 44100 Hz (Lanczos filter)
- **2× Downsampling:** 88200 → 44100 Hz (Lanczos filter)

### Raw Audio Support

If no WAV header detected:
- Assumes: 44100 Hz, 16-bit, mono
- Reads samples directly as int16_t

## Audio Stream Architecture

### Base Interface

```cpp
class AudioStream {
public:
    virtual int read(int16_t* buffer, int samples) = 0;
    virtual bool eof() const { return false; }
    virtual void stop() {}
};
```

### Key Implementations

#### 1. PlayWav - WAV File Decoder

**State Machine:**
```
IDLE → OPEN_FILE → READ_HEADER → PARSE_HEADER → READ_DATA → EOF
```

**read() method:**
```cpp
int PlayWav::read(int16_t* buffer, int samples) {
    int read_count = 0;

    while (read_count < samples && !eof()) {
        // Read from file buffer
        if (buffer_pos >= buffer_size) {
            refill_buffer();
        }

        // Convert based on format
        if (bits_per_sample == 16) {
            buffer[read_count] = ((int16_t*)file_buffer)[buffer_pos];
        } else {  // 8-bit
            buffer[read_count] = (file_buffer[buffer_pos] << 8) - 32768;
        }

        read_count++;
        buffer_pos++;
    }

    return read_count;
}
```

#### 2. BufferedAudioStream - Adds Lookahead Buffer

**Purpose:** Pre-read data to prevent SD card latency causing underruns

```cpp
template<int BUFFER_SIZE>
class BufferedAudioStream : public AudioStream {
private:
    AudioStream* source_;
    int16_t buffer_[BUFFER_SIZE];
    int read_pos_;
    int write_pos_;

public:
    int read(int16_t* output, int samples) {
        // Refill buffer in background
        while (space_available() && !source_->eof()) {
            int to_read = min(space_available(), 64);
            int read_count = source_->read(&buffer_[write_pos_], to_read);
            write_pos_ = (write_pos_ + read_count) % BUFFER_SIZE;
        }

        // Copy from buffer to output
        int samples_available = data_available();
        int to_copy = min(samples, samples_available);

        for (int i = 0; i < to_copy; i++) {
            output[i] = buffer_[read_pos_];
            read_pos_ = (read_pos_ + 1) % BUFFER_SIZE;
        }

        return to_copy;
    }
};
```

**Buffer Size:** 512 bytes (256 samples)

#### 3. VolumeOverlay - Smooth Volume Control

**Purpose:** Add fade in/out without clicks

```cpp
template<typename T>
class VolumeOverlay : public AudioStream {
private:
    T* source_;
    int volume_;           // 0-16384 (14-bit)
    int target_volume_;
    int fade_speed_;       // Units per callback

public:
    int read(int16_t* buffer, int samples) {
        int count = source_->read(buffer, samples);

        for (int i = 0; i < count; i++) {
            // Update volume with fade
            if (volume_ < target_volume_) {
                volume_ = min(volume_ + fade_speed_, target_volume_);
            } else if (volume_ > target_volume_) {
                volume_ = max(volume_ - fade_speed_, target_volume_);
            }

            // Apply volume (14-bit precision)
            buffer[i] = (buffer[i] * volume_) >> 14;
        }

        return count;
    }

    void set_fade_time(float seconds) {
        // Calculate speed to complete fade in given time
        int total_steps = seconds * 44100 / AUDIO_BUFFER_SIZE;
        fade_speed_ = 16384 / total_steps;
    }
};
```

**Volume Range:**
- 0 = silent
- 8192 = 50%
- 16384 = 100%

**Default Fade:** 0.2 seconds (200 ms)

#### 4. BufferedWavPlayer - Complete Player

**Composition:**
```cpp
class BufferedWavPlayer {
private:
    PlayWav decoder_;
    BufferedAudioStream<512> buffer_;
    VolumeOverlay<BufferedAudioStream<512>> volume_control_;

public:
    void play(const char* filename) {
        decoder_.open(filename);
        buffer_.set_source(&decoder_);
        volume_control_.set_source(&buffer_);
        volume_control_.set_volume(16384);  // 100%
    }

    int read(int16_t* output, int samples) {
        return volume_control_.read(output, samples);
    }
};
```

**Player Pool:** 7 players available (NUM_WAV_PLAYERS = 7)

## Audio Pipeline Data Flow

### Complete Flow Diagram

```
┌──────────────┐
│  SD Card     │
│  /font/      │
│  hum.wav     │
│  swing01.wav │
└──────┬───────┘
       │
       ▼
┌──────────────────┐     ┌──────────────────┐
│  Effect Class    │────▶│  File Manager    │
│  - Tracks files  │     │  - Opens files   │
│  - Random select │     └─────────┬────────┘
└──────────────────┘               │
                                   ▼
                          ┌─────────────────┐
                          │    PlayWav      │ ×7 players
                          │  - State machine│
                          │  - Format decode│
                          └────────┬────────┘
                                   │
                                   ▼
                          ┌─────────────────┐
                          │ BufferedStream  │ ×7
                          │  - 512 bytes    │
                          └────────┬────────┘
                                   │
                                   ▼
                          ┌─────────────────┐
                          │ VolumeOverlay   │ ×7
                          │  - Fade control │
                          └────────┬────────┘
                                   │
      ┌────────────────────────────┼────────────────┬─────────────┐
      │                            │                │             │
      ▼                            ▼                ▼             ▼
┌──────────┐              ┌──────────┐      ┌──────────┐  ┌──────────┐
│ Player 0 │              │ Player 1 │ ...  │ Player 6 │  │  Beeper  │
│  (Hum)   │              │ (Swing)  │      │ (Effect) │  │  (Menu)  │
└────┬─────┘              └────┬─────┘      └────┬─────┘  └────┬─────┘
     │                         │                 │             │
     └─────────────────────────┼─────────────────┴─────────────┘
                               │
                               ▼
                      ┌──────────────────┐
                      │ DynamicMixer<9>  │
                      │  - Sum streams   │
                      │  - Compression   │
                      │  - Volume control│
                      └────────┬─────────┘
                               │
                               ▼
                      ┌──────────────────┐
                      │ Optional Filter  │
                      │  - High-pass     │
                      │  - Butterworth   │
                      └────────┬─────────┘
                               │
                               ▼
                      ┌──────────────────┐
                      │  Audio ISR/Task  │
                      │  - Fill buffer   │
                      └────────┬─────────┘
                               │
                               ▼
                      ┌──────────────────┐
                      │   DMA Buffer     │
                      │  - Double buffer │
                      └────────┬─────────┘
                               │
                               ▼
                      ┌──────────────────┐
                      │  I2S Hardware    │
                      └────────┬─────────┘
                               │
                               ▼
                      ┌──────────────────┐
                      │    DAC/Amp       │
                      │   (Speaker)      │
                      └──────────────────┘
```

## Implementation for ESP32

### Recommended Architecture

**FreeRTOS Task-Based:**

```cpp
// Audio output task (high priority)
void audio_task(void* param) {
    int16_t buffer[AUDIO_BUFFER_SIZE];
    size_t bytes_written;

    while (1) {
        // Fill buffer from mixer
        int samples = dynamic_mixer.read(buffer, AUDIO_BUFFER_SIZE);

        // Pad with zeros if underrun
        if (samples < AUDIO_BUFFER_SIZE) {
            memset(&buffer[samples], 0, (AUDIO_BUFFER_SIZE - samples) * 2);
        }

        // Send to I2S (blocks until DMA ready)
        i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &bytes_written, portMAX_DELAY);
    }
}

// File read task (lower priority)
void file_task(void* param) {
    while (1) {
        // Refill player buffers
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].needs_data()) {
                players[i].refill_buffer();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // Yield CPU
    }
}
```

**Task Priorities:**
- Audio output: Priority 3 (highest)
- File read: Priority 2
- Motion processing: Priority 1
- Main loop: Priority 0

### Memory Allocation

**Static Allocation (Recommended):**
```cpp
// Audio buffers
int16_t dma_buffer[AUDIO_BUFFER_SIZE * 2] __attribute__((aligned(4)));
int16_t player_buffers[NUM_PLAYERS][512] __attribute__((aligned(4)));
uint8_t file_read_buffer[512] __attribute__((aligned(4)));

// Total: ~10 KB
```

**Heap Usage:**
- WAV player objects: ~200 bytes each × 7 = 1.4 KB
- Mixer object: ~500 bytes
- Total: ~2 KB

### Performance Optimization

**DMA Benefits:**
- Zero CPU during audio transmission
- Accurate timing (hardware-driven)
- Allows CPU for motion processing and file I/O

**Buffer Tuning:**
- Larger buffers: More latency, less CPU load
- Smaller buffers: Less latency, more CPU load
- Recommended: 44 samples (1 ms) for responsiveness

**File System:**
- Use SDMMC (4-bit) for faster SD access than SPI
- Pre-cache directory entries to speed file opening
- Keep files defragmented

## Optional Audio Processing

### High-Pass Filter

**Purpose:** Remove DC offset and low-frequency rumble

**Implementation:** 8th-order Butterworth cascaded biquads

**Pseudocode:**
```cpp
class ButterworthFilter {
private:
    struct Biquad {
        float b0, b1, b2, a1, a2;
        float x1, x2, y1, y2;
    };

    Biquad sections[4];  // 8th order = 4 biquad sections

public:
    float process(float input) {
        float output = input;
        for (int i = 0; i < 4; i++) {
            float temp = sections[i].b0 * output
                       + sections[i].b1 * sections[i].x1
                       + sections[i].b2 * sections[i].x2
                       - sections[i].a1 * sections[i].y1
                       - sections[i].a2 * sections[i].y2;

            sections[i].x2 = sections[i].x1;
            sections[i].x1 = output;
            sections[i].y2 = sections[i].y1;
            sections[i].y1 = temp;

            output = temp;
        }
        return output;
    }
};
```

**Cutoff Frequency:** Typically 40-100 Hz

## Summary of Key Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Sample Rate** | 44100 Hz | Fixed output rate |
| **Bit Depth** | 16-bit | Signed integer |
| **Channels** | 1 (mono) | Internal mixing |
| **Buffer Size** | 44 samples | 1 ms per buffer |
| **DMA Buffers** | 2 × 88 samples | Double buffering |
| **Player Buffers** | 512 bytes each | Per-player lookahead |
| **File Read Buffer** | 512 bytes | SD card efficiency |
| **Number of Players** | 7 | Simultaneous sounds |
| **Volume Precision** | 14-bit | 0-16384 range |
| **Fade Time** | 200 ms | Default smooth fade |

## Next Steps

For complete audio integration:
- **Audio Mixing:** See `audio-mixing-pipeline.md`
- **SmoothSwing Integration:** See `smoothswing-algorithm.md`
- **Sound Font Requirements:** See `sound-font-structure.md`
