#pragma once

#include "esp_err.h"

namespace Espressif::App {

/**
 * @brief Interface for modular application samples.
 */
class ISample {
public:
    /**
     * @brief Virtual destructor for polymorphic cleanup.
     */
    virtual ~ISample() = default;

    /**
     * @brief Perform horizontal hardware initialization for the sample.
     * @return esp_err_t ESP_OK on success.
     */
    virtual esp_err_t setup() = 0;

    /**
     * @brief Execute the sample logic (can be a loop or a one-shot task).
     */
    virtual void run() = 0;
};

} // namespace Espressif::App
