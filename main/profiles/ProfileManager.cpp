#include "profiles/ProfileManager.hpp"
#include "profiles/ProfileLoader.hpp"
#include "esp_log.h"
#include <optional>

namespace InertialSaber::Profiles {

static constexpr const char* TAG = "ProfileManager";

esp_err_t ProfileManager::init() {
    ESP_LOGI(TAG, "Initializing profiles...");

    const esp_err_t err = ProfileLoader::loadFromSd(m_profiles);

    if (m_profiles.empty()) {
        ESP_LOGE(TAG, "No valid profile on SD (scan: %s)", esp_err_to_name(err));
        ESP_LOGE(TAG, "Each profile needs /sdcard/profiles/<name>/profile.json");
        return err != ESP_OK ? err : ESP_ERR_NOT_FOUND;
    }

    m_activeIndex = 0;
    if (const std::optional<size_t> storedIndex = m_store.load(); storedIndex.has_value()) {
        if (*storedIndex < m_profiles.size()) {
            m_activeIndex = *storedIndex;
            ESP_LOGI(TAG, "Restored active profile index: %u",
                     static_cast<unsigned>(m_activeIndex));
        } else {
            ESP_LOGW(TAG, "Loaded active index %u out of bounds (%u profiles). Resetting to 0.",
                     static_cast<unsigned>(*storedIndex), static_cast<unsigned>(m_profiles.size()));
        }
    }
    ESP_LOGI(TAG, "Initialized %u profile(s), active index: %u",
             static_cast<unsigned>(m_profiles.size()), static_cast<unsigned>(m_activeIndex));
    return ESP_OK;
}

esp_err_t ProfileManager::loadActive() {
    if (m_profiles.empty()) return ESP_ERR_INVALID_STATE;
    m_profiles[m_activeIndex]->load(m_services, *this);
    logActiveProfile();
    return ESP_OK;
}

void ProfileManager::next() {
    if (m_profiles.empty()) return;

    const size_t previousIndex = m_activeIndex;
    ESP_LOGD(TAG, "Hot-swapping profile: unloading active index %u",
             static_cast<unsigned>(m_activeIndex));
    m_profiles[m_activeIndex]->unload(m_services);

    m_activeIndex = (m_activeIndex + 1) % m_profiles.size();

    ESP_LOGD(TAG, "Loading next profile at index %u...", static_cast<unsigned>(m_activeIndex));
    m_profiles[m_activeIndex]->load(m_services, *this);
    logActiveProfile();
    if (m_activeIndex != previousIndex) {
        m_store.saveAsync(m_activeIndex);
    }
}

void ProfileManager::logActiveProfile() const {
    ESP_LOGI(TAG, "Active profile %u/%u: %s", static_cast<unsigned>(m_activeIndex + 1),
             static_cast<unsigned>(m_profiles.size()),
             m_profiles[m_activeIndex]->definition().profileName.c_str());
}

} // namespace InertialSaber::Profiles
