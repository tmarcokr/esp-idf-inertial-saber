#pragma once

#include "core/BusConfig.hpp"
#include "system/board/Board.hpp"

#include "AudioEngine.hpp"
#include "sd_card.hpp"

#include "esp_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <cstdint>

namespace InertialSaber::System::Hardware {

/**
 * @brief FreeRTOS creation parameters of a task; a core of tskNO_AFFINITY means unpinned.
 */
struct TaskSpec {
    const char* name;
    uint32_t stackSize;
    UBaseType_t priority;
    BaseType_t core;
};

/**
 * @brief Stack size, priority and core of every task in the firmware.
 *
 * Tasks owned by main/ are created from these entries. Entries documented as reference only
 * describe tasks that components or ESP-IDF create with fixed parameters; they are not applied
 * and only feed the metrics report.
 */
struct TaskTable {
    static constexpr TaskSpec kBus{
        .name = "saber_bus", .stackSize = 8192, .priority = 8, .core = 0};
    static constexpr TaskSpec kImuAdapter{
        .name = "imu_adapter", .stackSize = 4096, .priority = 9, .core = 0};
    static constexpr TaskSpec kAudioControl{
        .name = "audio_ctrl", .stackSize = 4096, .priority = 7, .core = 1};
    static constexpr TaskSpec kPsramLoader{
        .name = "psram_loader", .stackSize = 4096, .priority = 2, .core = 1};
    static constexpr TaskSpec kProfileStore{
        .name = "profile_store", .stackSize = 3072, .priority = 1, .core = 1};
    static constexpr TaskSpec kMetricsReporter{
        .name = "metrics", .stackSize = 4096, .priority = 1, .core = 1};

    /** @brief Stack and priority applied through SmartLed::Engine::start(); core not settable. */
    static constexpr TaskSpec kSmartLed{
        .name = "SmartLedTask", .stackSize = 4096, .priority = 5, .core = tskNO_AFFINITY};

    /** @brief Reference only: created by GpioButton. */
    static constexpr TaskSpec kButtonPoll{
        .name = "gpio_btn_tsk", .stackSize = 4096, .priority = 5, .core = tskNO_AFFINITY};
    /** @brief Reference only: created by AudioEngine. */
    static constexpr TaskSpec kAudioMixer{
        .name = "audio_mixer", .stackSize = 4096, .priority = 10, .core = 1};
    /** @brief Reference only: created by AudioEngine as "audio_mem_reader", name truncated. */
    static constexpr TaskSpec kAudioMemReader{
        .name = "audio_mem_reade", .stackSize = 4096, .priority = 9, .core = 1};
    /** @brief Reference only: created by AudioEngine. */
    static constexpr TaskSpec kAudioSdReader{
        .name = "audio_sd_reader", .stackSize = 8192, .priority = 6, .core = 1};
    /** @brief Reference only: ESP-IDF timer task, sized and pinned through sdkconfig. */
    static constexpr TaskSpec kEspTimer{.name = "esp_timer",
                                        .stackSize = ESP_TASK_TIMER_STACK,
                                        .priority = ESP_TASK_TIMER_PRIO,
                                        .core = CONFIG_ESP_TIMER_TASK_AFFINITY};
    /** @brief Reference only: ESP-IDF main task, sized and pinned through sdkconfig. */
    static constexpr TaskSpec kMain{.name = "main",
                                    .stackSize = ESP_TASK_MAIN_STACK,
                                    .priority = ESP_TASK_MAIN_PRIO,
                                    .core = ESP_TASK_MAIN_CORE};
};

static_assert(TaskTable::kImuAdapter.priority > TaskTable::kBus.priority &&
              TaskTable::kImuAdapter.core == TaskTable::kBus.core);

struct HardwareConfig {
    static constexpr Core::BusConfig kBusConfig{
        .task = {.name = TaskTable::kBus.name,
                 .stackSize = TaskTable::kBus.stackSize,
                 .priority = TaskTable::kBus.priority,
                 .core = TaskTable::kBus.core},
        .motion = {.warmUpPeriodMs = 3000, .orientationOffsetDeg = 0.0f},
    };

    static constexpr uint32_t kClickWindowMs = 400;
    static constexpr uint32_t kHoldTickMs = 500;

    static constexpr uint16_t kNumLeds = 5;

    // Compressor threshold and DC cutoff are tuned for the MAX98357A on this hardware.
    static constexpr uint32_t kAudioSampleRate = 44100;
    static constexpr uint8_t kAudioMaxChannels = 9;
    static constexpr uint16_t kAudioCompressorThreshold = CONFIG_SABER_AUDIO_COMPRESSOR_THRESHOLD;
    static constexpr auto kAudioDcCutoff =
        Espressif::Wrappers::Audio::DcBlocker::CutoffPreset::Hz50;
    static constexpr uint16_t kAudioGlobalVolume = 16384;

    static constexpr uint8_t kBladeBrightness = 255;
    static constexpr uint32_t kBladeTargetFps = 100;

    static constexpr const char* kSdMountPoint = "/sdcard";
    static constexpr int kSdMaxFiles = 16;
};

inline Espressif::Wrappers::SdCard::Config makeSdConfig() {
    return {
        .mode = Espressif::Wrappers::SdCard::HostMode::SDMMC_1BIT,
        .clk = Board::kPins.sdClk,
        .cmd = Board::kPins.sdCmd,
        .d0 = Board::kPins.sdD0,
        .mount_point = HardwareConfig::kSdMountPoint,
        .max_files = HardwareConfig::kSdMaxFiles,
        .format_if_mount_failed = false,
    };
}

inline constexpr Espressif::Wrappers::Audio::AudioEngine::Config makeAudioConfig() {
    return {
        .bclk_pin = Board::kPins.i2sBclk,
        .ws_pin = Board::kPins.i2sWs,
        .dout_pin = Board::kPins.i2sDout,
        .sd_mode_pin = Board::kPins.i2sSdMode,
        .sample_rate = HardwareConfig::kAudioSampleRate,
        .max_channels = HardwareConfig::kAudioMaxChannels,
        .compressor_gain_threshold = HardwareConfig::kAudioCompressorThreshold,
        .dc_cutoff = HardwareConfig::kAudioDcCutoff,
    };
}

} // namespace InertialSaber::System::Hardware
