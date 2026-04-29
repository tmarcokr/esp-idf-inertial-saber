#pragma once

#include "sdkconfig.h"
#include <cstdint>

namespace InertialSaber::Core::Platform {

#if CONFIG_IDF_TARGET_ESP32S3
    constexpr int kBusTaskCore = 0;
    constexpr int kEngineTaskCore = 1;
#else
    // ESP32-C6 and other single-core targets
    constexpr int kBusTaskCore = 0;
    constexpr int kEngineTaskCore = 0;
#endif

constexpr uint8_t kBusTaskPriority = 8;
constexpr uint32_t kBusTaskStackSize = 4096;
constexpr uint8_t kMaxInputs = 4;
constexpr uint32_t kBusTimeoutMs = 2;
constexpr uint8_t kInputQueueDepth = 8;

} // namespace InertialSaber::Core::Platform
