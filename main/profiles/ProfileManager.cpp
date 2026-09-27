#include "profiles/ProfileManager.hpp"
#include "profiles/ProfileLoader.hpp"
#include "system/Raii.hpp"
#include "esp_log.h"
#include <cstdio>
#include <utility>

namespace InertialSaber::Profiles {

static constexpr const char* TAG = "ProfileManager";
static constexpr const char* kActiveProfilePath = "/sdcard/active_profile.txt";

esp_err_t ProfileManager::init() {
    ESP_LOGI(TAG, "Initializing profiles...");

    const esp_err_t err = ProfileLoader::loadFromSd(m_profiles);

    if (m_profiles.empty()) {
        ESP_LOGE(TAG, "No valid profile on SD (scan: %s)", esp_err_to_name(err));
        ESP_LOGE(TAG, "Each profile needs /sdcard/profiles/<name>/profile.json");
        return err != ESP_OK ? err : ESP_ERR_NOT_FOUND;
    }

    m_activeIndex = 0;
    if (System::UniqueFile file = System::openFile(kActiveProfilePath, "r")) {
        unsigned int loadedIndex = 0;
        if (fscanf(file.get(), "%u", &loadedIndex) == 1) {
            if (loadedIndex < m_profiles.size()) {
                m_activeIndex = loadedIndex;
                ESP_LOGI(TAG, "Restored active profile index: %u", loadedIndex);
            } else {
                ESP_LOGW(TAG, "Loaded active index %u out of bounds (%u profiles). Resetting to 0.",
                         loadedIndex, static_cast<unsigned>(m_profiles.size()));
            }
        } else {
            ESP_LOGW(TAG, "Failed to parse active_profile.txt content");
        }
    } else {
        ESP_LOGW(TAG, "active_profile.txt not found, defaulting to index 0");
    }
    ESP_LOGI(TAG, "Initialized %u profile(s), active index: %u",
             static_cast<unsigned>(m_profiles.size()), static_cast<unsigned>(m_activeIndex));
    return ESP_OK;
}

esp_err_t ProfileManager::loadActive() {
    if (m_profiles.empty()) return ESP_ERR_INVALID_STATE;
    m_profiles[m_activeIndex]->load(m_services, *this);
    return ESP_OK;
}

void ProfileManager::next() {
    if (m_profiles.empty()) return;

    const size_t previousIndex = m_activeIndex;
    ESP_LOGI(TAG, "Hot-swapping profile: unloading active index %u", m_activeIndex);
    m_profiles[m_activeIndex]->unload(m_services);

    m_activeIndex = (m_activeIndex + 1) % m_profiles.size();

    ESP_LOGI(TAG, "Loading next profile at index %u...", m_activeIndex);
    m_profiles[m_activeIndex]->load(m_services, *this);
    if (m_activeIndex != previousIndex) {
        saveActiveIndex();
    }
}

void ProfileManager::saveActiveIndex() {
    System::UniqueFile file = System::openFile(kActiveProfilePath, "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open active_profile.txt for writing");
        return;
    }
    const bool written = fprintf(file.get(), "%u\n", static_cast<unsigned>(m_activeIndex)) > 0;
    const bool closed = System::closeFile(std::move(file));
    if (!written || !closed) {
        ESP_LOGE(TAG, "Failed to write active_profile.txt");
        return;
    }
    ESP_LOGI(TAG, "Saved active profile index: %u", static_cast<unsigned>(m_activeIndex));
}

} // namespace InertialSaber::Profiles
