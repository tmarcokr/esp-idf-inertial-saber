#include "system/persistence/ActiveProfileStore.hpp"
#include "system/Raii.hpp"

#include "esp_log.h"

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <utility>

namespace InertialSaber::System {

namespace {

constexpr const char* TAG = "ActiveProfileStore";
constexpr const char* kActiveProfilePath = "/sdcard/active_profile.txt";

} // namespace

std::optional<size_t> ActiveProfileStore::load() const {
    UniqueFile file = openFile(kActiveProfilePath, "r");
    if (!file) {
        ESP_LOGW(TAG, "active_profile.txt not found, defaulting to index 0");
        return std::nullopt;
    }
    uint32_t index = 0;
    if (fscanf(file.get(), "%" SCNu32, &index) != 1) {
        ESP_LOGW(TAG, "Failed to parse active_profile.txt content");
        return std::nullopt;
    }
    return index;
}

esp_err_t ActiveProfileStore::save(size_t index) const {
    if (!std::in_range<uint32_t>(index)) return ESP_ERR_INVALID_ARG;
    const auto value = static_cast<uint32_t>(index);

    UniqueFile file = openFile(kActiveProfilePath, "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open active_profile.txt for writing");
        return ESP_FAIL;
    }
    const bool written = fprintf(file.get(), "%" PRIu32 "\n", value) > 0;
    const bool closed = closeFile(std::move(file));
    if (!written || !closed) {
        ESP_LOGE(TAG, "Failed to write active_profile.txt");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Saved active profile index: %" PRIu32, value);
    return ESP_OK;
}

} // namespace InertialSaber::System
