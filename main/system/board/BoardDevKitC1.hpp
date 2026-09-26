#pragma once

#include "system/status/RgbStatusIndicator.hpp"

#include "driver/gpio.h"

#include <string_view>

namespace InertialSaber::System::Board {

/**
 * @brief GPIO assignment of one board variant.
 */
struct BoardPins {
    gpio_num_t imuSda;
    gpio_num_t imuScl;
    gpio_num_t imuInt;
    gpio_num_t mainButton;
    gpio_num_t sdClk;
    gpio_num_t sdCmd;
    gpio_num_t sdD0;
    gpio_num_t i2sBclk;
    gpio_num_t i2sWs;
    gpio_num_t i2sDout;
    gpio_num_t i2sSdMode;
    gpio_num_t bladeData;
};

inline constexpr std::string_view kName = "ESP32-S3-DevKitC-1";

inline constexpr BoardPins kPins{
    .imuSda     = GPIO_NUM_4,
    .imuScl     = GPIO_NUM_5,
    .imuInt     = GPIO_NUM_6,
    .mainButton = GPIO_NUM_0,
    .sdClk      = GPIO_NUM_9,
    .sdCmd      = GPIO_NUM_8,
    .sdD0       = GPIO_NUM_7,
    .i2sBclk    = GPIO_NUM_11,
    .i2sWs      = GPIO_NUM_12,
    .i2sDout    = GPIO_NUM_13,
    .i2sSdMode  = GPIO_NUM_14,
    .bladeData  = GPIO_NUM_21,
};

inline constexpr bool kMainButtonActiveLow = true;

using StatusIndicatorType = Status::RgbStatusIndicator;
inline constexpr Status::RgbStatusIndicator::Config kStatusIndicatorConfig{.pin = GPIO_NUM_48};

} // namespace InertialSaber::System::Board
