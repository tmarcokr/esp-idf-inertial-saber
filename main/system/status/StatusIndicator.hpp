#pragma once

#include "esp_err.h"

#include <cstdint>

namespace InertialSaber::System::Status {

/**
 * @brief Semantic system states rendered by a board-specific indicator.
 */
enum class SystemStatus : uint8_t { Booting, Preloading, Ready, Error };

/**
 * @brief Board-independent status output.
 *
 * Not thread-safe: called from the main task during start-up and from the bus task afterwards, never concurrently.
 * Animated states (Preloading) require show() to be called periodically; show() is idempotent.
 */
class StatusIndicator {
public:
    virtual ~StatusIndicator() = default;

    /**
     * @brief Initialize the underlying output peripheral.
     */
    [[nodiscard]] virtual esp_err_t init() = 0;

    /**
     * @brief Render the given status.
     */
    virtual void show(SystemStatus status) = 0;
};

} // namespace InertialSaber::System::Status
