#pragma once

#include "sdkconfig.h"
#include "driver/gpio.h"
#include <cstdint>

namespace InertialSaber::System::Hardware {

struct HardwareConfig {
#if CONFIG_IDF_TARGET_ESP32S3
    // ── ESP32-S3 Hardware Pinout ──
    // IMU (MPU6050)
    static constexpr gpio_num_t kImuSda = GPIO_NUM_4;
    static constexpr gpio_num_t kImuScl = GPIO_NUM_5;
    static constexpr gpio_num_t kImuInt = GPIO_NUM_6;

    // Main Button
    static constexpr gpio_num_t kMainBtn = GPIO_NUM_0;

    // Status LED (Internal WS2812)
    static constexpr gpio_num_t kStatusLed = GPIO_NUM_48;

    // SD Card SDMMC (1-Bit)
    static constexpr gpio_num_t kSdD0   = GPIO_NUM_7;
    static constexpr gpio_num_t kSdCmd  = GPIO_NUM_8;
    static constexpr gpio_num_t kSdClk  = GPIO_NUM_9;

    // I2S / MAX98357A
    static constexpr gpio_num_t kI2sBclk   = GPIO_NUM_11;
    static constexpr gpio_num_t kI2sWs     = GPIO_NUM_12;
    static constexpr gpio_num_t kI2sDout   = GPIO_NUM_13;
    static constexpr gpio_num_t kI2sSdMode = GPIO_NUM_14;

    // SmartLed
    static constexpr gpio_num_t kLedData = GPIO_NUM_21;

    // Task Scheduling
    static constexpr int kBusTaskCore    = 0;
    static constexpr int kEngineTaskCore = 1;
#else
    // ── ESP32-C6 Hardware Pinout (Default) ──
    // IMU (MPU6050)
    static constexpr gpio_num_t kImuSda = GPIO_NUM_22;
    static constexpr gpio_num_t kImuScl = GPIO_NUM_23;
    static constexpr gpio_num_t kImuInt = GPIO_NUM_21;

    // Main Button
    static constexpr gpio_num_t kMainBtn = GPIO_NUM_9;

    // Status LED (Internal WS2812)
    static constexpr gpio_num_t kStatusLed = GPIO_NUM_8;

    // SD Card SPI
    static constexpr gpio_num_t kSdMiso = GPIO_NUM_4;
    static constexpr gpio_num_t kSdMosi = GPIO_NUM_11;
    static constexpr gpio_num_t kSdSck  = GPIO_NUM_7;
    static constexpr gpio_num_t kSdCs   = GPIO_NUM_10;

    // I2S / MAX98357A
    static constexpr gpio_num_t kI2sBclk   = GPIO_NUM_18;
    static constexpr gpio_num_t kI2sWs     = GPIO_NUM_19;
    static constexpr gpio_num_t kI2sDout   = GPIO_NUM_20;
    static constexpr gpio_num_t kI2sSdMode = GPIO_NUM_1;

    // SmartLed
    static constexpr gpio_num_t kLedData = GPIO_NUM_0;

    // Task Scheduling
    static constexpr int kBusTaskCore    = 0;
    static constexpr int kEngineTaskCore = 0;
#endif

    // ── Common Parameters ──
    static constexpr uint32_t kImuGracePeriodMs = 3000;
    static constexpr float kImuOrientationOffsetDeg = 0.0f;

    static constexpr uint8_t kMainBtnInputId = 0;
    static constexpr uint32_t kClickWindowMs = 400;
    static constexpr uint32_t kHoldTickMs = 500;

    static constexpr uint16_t kNumLeds   = 5;

    static constexpr uint8_t  kBusTaskPriority  = 8;
    static constexpr uint32_t kBusTaskStackSize = 8192;
    static constexpr uint8_t  kMaxInputs        = 4;
};

} // namespace InertialSaber::System::Hardware
