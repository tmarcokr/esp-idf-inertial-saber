#include "profiles/ProfileManager.hpp"
#include "profiles/ProfileLoader.hpp"
#include "esp_log.h"
#include <cstdio>

namespace InertialSaber::Profiles {

static constexpr const char *TAG = "ProfileManager";

esp_err_t ProfileManager::init() {
  ESP_LOGI(TAG, "Initializing profiles...");

  const esp_err_t err = ProfileLoader::loadFromSd(m_profiles);

  if (m_profiles.empty()) {
    ESP_LOGE(TAG, "No valid profile on SD (scan: %s). Each profile needs /sdcard/profiles/<name>/profile.json",
             esp_err_to_name(err));
    return err != ESP_OK ? err : ESP_ERR_NOT_FOUND;
  }

  m_activeIndex = 0;
  FILE *f = fopen("/sdcard/active_profile.txt", "r");
  if (f) {
    unsigned int loadedIndex = 0;
    if (fscanf(f, "%u", &loadedIndex) == 1) {
      if (loadedIndex < m_profiles.size()) {
        m_activeIndex = loadedIndex;
        ESP_LOGI(TAG, "Restored active profile index: %u", loadedIndex);
      } else {
        ESP_LOGW(TAG, "Loaded active index %u out of bounds (%u profiles). Resetting to 0.", loadedIndex, (unsigned)m_profiles.size());
      }
    } else {
      ESP_LOGW(TAG, "Failed to parse active_profile.txt content");
    }
    fclose(f);
  } else {
    ESP_LOGW(TAG, "active_profile.txt not found, defaulting to index 0");
  }
  ESP_LOGI(TAG, "Initialized %u profile(s), active index: %u", (unsigned)m_profiles.size(), (unsigned)m_activeIndex);
  return ESP_OK;
}

esp_err_t ProfileManager::loadActive() {
  if (m_profiles.empty()) return ESP_ERR_INVALID_STATE;
  m_profiles[m_activeIndex]->load(m_services, *this);
  return ESP_OK;
}

void ProfileManager::next() {
  if (m_profiles.size() <= 1) return;

  ESP_LOGI(TAG, "Hot-swapping profile: unloading active index %u", m_activeIndex);
  m_profiles[m_activeIndex]->unload(m_services);

  m_activeIndex = (m_activeIndex + 1) % m_profiles.size();

  ESP_LOGI(TAG, "Loading next profile at index %u...", m_activeIndex);
  m_profiles[m_activeIndex]->load(m_services, *this);
  saveActiveIndex();
}

void ProfileManager::saveActiveIndex() {
  FILE *f = fopen("/sdcard/active_profile.txt", "w");
  if (f) {
    fprintf(f, "%u\n", (unsigned int)m_activeIndex);
    fclose(f);
    ESP_LOGI(TAG, "Saved active profile index: %u", (unsigned)m_activeIndex);
  } else {
    ESP_LOGE(TAG, "Failed to open active_profile.txt for writing");
  }
}

} // namespace InertialSaber::Profiles
