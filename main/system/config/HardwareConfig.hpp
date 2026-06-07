#pragma once

#include "sdkconfig.h"
#include "driver/gpio.h"
#include <cstdint>

namespace InertialSaber::System::Config {

struct HardwareConfig {
    // ── IMU (MPU6050) ──
    static constexpr gpio_num_t kImuSda = GPIO_NUM_22;
    static constexpr gpio_num_t kImuScl = GPIO_NUM_23;
    static constexpr gpio_num_t kImuInt = GPIO_NUM_21;
    static constexpr uint32_t kImuGracePeriodMs = 3000;
    static constexpr float kImuOrientationOffsetDeg = 0.0f;

    // ── Main Button ──
    static constexpr gpio_num_t kMainBtn = GPIO_NUM_9;
    static constexpr uint8_t kMainBtnInputId = 0;
    static constexpr uint32_t kClickWindowMs = 400;
    static constexpr uint32_t kHoldTickMs = 500;

    // ── SD Card SPI pins ──
    static constexpr gpio_num_t kSdMiso = GPIO_NUM_4;
    static constexpr gpio_num_t kSdMosi = GPIO_NUM_11;
    static constexpr gpio_num_t kSdSck  = GPIO_NUM_7;
    static constexpr gpio_num_t kSdCs   = GPIO_NUM_10;

    // ── I2S / MAX98357A pins ──
    static constexpr gpio_num_t kI2sBclk   = GPIO_NUM_18;
    static constexpr gpio_num_t kI2sWs     = GPIO_NUM_19;
    static constexpr gpio_num_t kI2sDout   = GPIO_NUM_20;
    static constexpr gpio_num_t kI2sSdMode = GPIO_NUM_1;

    // ── SmartLed pins ──
    static constexpr gpio_num_t kLedData = GPIO_NUM_0;
    static constexpr uint16_t kNumLeds   = 5;

    // ── Task Scheduling ──
#if CONFIG_IDF_TARGET_ESP32S3
    static constexpr int kBusTaskCore    = 0;
    static constexpr int kEngineTaskCore = 1;
#else
    static constexpr int kBusTaskCore    = 0;
    static constexpr int kEngineTaskCore = 0;
#endif
    static constexpr uint8_t  kBusTaskPriority  = 8;
    static constexpr uint32_t kBusTaskStackSize = 8192;
    static constexpr uint8_t  kMaxInputs        = 4;
};

} // namespace InertialSaber::System::Config
