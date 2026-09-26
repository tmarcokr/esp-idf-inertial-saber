#pragma once

#include "core/BusConfig.hpp"
#include "system/board/Board.hpp"

#include "AudioEngine.hpp"
#include "sd_card.hpp"

#include <cstdint>

namespace InertialSaber::System::Hardware {

struct HardwareConfig {
    static constexpr Core::BusConfig kBusConfig{
        .task   = {.stackSize = 8192, .priority = 8, .core = 0},
        .motion = {.warmUpPeriodMs = 3000, .orientationOffsetDeg = 0.0f},
    };
    static constexpr UBaseType_t kImuAdapterPriority = kBusConfig.task.priority + 1;
    static constexpr BaseType_t kImuAdapterCore = kBusConfig.task.core;

    static constexpr uint32_t kClickWindowMs = 400;
    static constexpr uint32_t kHoldTickMs = 500;

    static constexpr uint16_t kNumLeds = 5;

    // Compressor threshold and DC cutoff are tuned for the MAX98357A on this hardware.
    static constexpr uint32_t kAudioSampleRate = 44100;
    static constexpr uint8_t kAudioMaxChannels = 9;
    static constexpr uint16_t kAudioCompressorThreshold = 1000;
    static constexpr auto kAudioDcCutoff = Espressif::Wrappers::Audio::DcBlocker::CutoffPreset::Hz50;
    static constexpr uint16_t kAudioGlobalVolume = 16384;

    static constexpr uint8_t kBladeBrightness = 255;
    static constexpr uint32_t kBladeTargetFps = 100;

    static constexpr const char* kSdMountPoint = "/sdcard";
    static constexpr int kSdMaxFiles = 16;
};

inline Espressif::Wrappers::SdCard::Config makeSdConfig() {
    return {
        .mode                   = Espressif::Wrappers::SdCard::HostMode::SDMMC_1BIT,
        .clk                    = Board::kPins.sdClk,
        .cmd                    = Board::kPins.sdCmd,
        .d0                     = Board::kPins.sdD0,
        .mount_point            = HardwareConfig::kSdMountPoint,
        .max_files              = HardwareConfig::kSdMaxFiles,
        .format_if_mount_failed = false,
    };
}

inline constexpr Espressif::Wrappers::Audio::AudioEngine::Config makeAudioConfig() {
    return {
        .bclk_pin                  = Board::kPins.i2sBclk,
        .ws_pin                    = Board::kPins.i2sWs,
        .dout_pin                  = Board::kPins.i2sDout,
        .sd_mode_pin               = Board::kPins.i2sSdMode,
        .sample_rate               = HardwareConfig::kAudioSampleRate,
        .max_channels              = HardwareConfig::kAudioMaxChannels,
        .compressor_gain_threshold = HardwareConfig::kAudioCompressorThreshold,
        .dc_cutoff                 = HardwareConfig::kAudioDcCutoff,
    };
}

} // namespace InertialSaber::System::Hardware
