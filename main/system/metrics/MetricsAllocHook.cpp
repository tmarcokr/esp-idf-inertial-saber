// SPDX-License-Identifier: GPL-3.0-or-later

#include "sdkconfig.h"

#if CONFIG_SABER_METRICS && CONFIG_HEAP_USE_HOOKS

#include "diagnostics/Metrics.hpp"

#include "esp_heap_caps.h"

extern "C" void esp_heap_trace_alloc_hook(void*, size_t, uint32_t) {
    InertialSaber::Diagnostics::Metrics::noteAllocation();
}

#endif
