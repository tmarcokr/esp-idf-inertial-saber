#pragma once

#include "esp_err.h"
#include "sdkconfig.h"

#include <cstdint>

namespace InertialSaber::System::Status {

/**
 * @brief Semantic system states rendered by a board-specific indicator.
 */
enum class SystemStatus : uint8_t { Booting, Preloading, Ready, Error };

#if CONFIG_SABER_METRICS
/**
 * @brief Transient storage activity rendered over the system status in metrics builds:
 * Writing while a write is in progress, then Written or WriteFailed for a while after it ends.
 */
enum class ActivitySignal : uint8_t { None, Writing, Written, WriteFailed };
#endif

/**
 * @brief Board-independent status output.
 *
 * show() is called from the main task during start-up and from the bus task afterwards, never
 * concurrently. In metrics builds showActivity() is called from the metrics reporter task;
 * implementations serialise it with show().
 * Animated states (Preloading, ActivitySignal::Written and ActivitySignal::WriteFailed) require
 * periodic calls; both methods are idempotent.
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

#if CONFIG_SABER_METRICS
    /**
     * @brief Render @p signal over the current status; ActivitySignal::None restores the status.
     */
    virtual void showActivity(ActivitySignal signal) = 0;
#endif
};

} // namespace InertialSaber::System::Status
