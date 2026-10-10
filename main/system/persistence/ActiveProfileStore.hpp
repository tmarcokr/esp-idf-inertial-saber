// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "esp_err.h"

#include <cstddef>
#include <optional>

namespace InertialSaber::System {

/**
 * @brief Persists the index of the active profile on the SD card.
 *
 * Both calls block on SD I/O; keep them off the real-time tasks.
 */
class ActiveProfileStore {
public:
    ActiveProfileStore() = default;

    ActiveProfileStore(const ActiveProfileStore&) = delete;
    ActiveProfileStore& operator=(const ActiveProfileStore&) = delete;

    /** @brief Reads the stored index; empty if the file is missing or unreadable. Blocking. */
    [[nodiscard]] std::optional<size_t> load() const;

    /**
     * @brief Writes @p index to /sdcard/active_profile.txt. Blocking; call from profile_ctrl only.
     * @return ESP_OK; ESP_ERR_INVALID_ARG if @p index exceeds UINT32_MAX; ESP_FAIL on an I/O error.
     */
    [[nodiscard]] esp_err_t save(size_t index) const;
};

} // namespace InertialSaber::System
