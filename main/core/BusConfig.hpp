#pragma once

#include "freertos/FreeRTOS.h"

#include <array>
#include <cstdint>

namespace InertialSaber::Core {

/**
 * @brief FreeRTOS parameters of the bus task.
 */
struct BusTaskConfig {
    uint32_t stackSize;
    UBaseType_t priority;
    BaseType_t core;
};

/**
 * @brief Motion conditioning applied by the bus before effects are evaluated.
 */
struct MotionFilterConfig {
    uint32_t warmUpPeriodMs;
    float orientationOffsetDeg;
};

/**
 * @brief Complete bus configuration injected by the composition root.
 */
struct BusConfig {
    BusTaskConfig task;
    MotionFilterConfig motion;
};

/**
 * @brief Raw motion sample published by an IMU adapter.
 */
struct MotionSample {
    float kineticEnergyG;
    std::array<float, 3> axisRotationDps;
    float orientationDeg;
};

} // namespace InertialSaber::Core
