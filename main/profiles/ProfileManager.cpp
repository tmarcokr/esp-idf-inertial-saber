#include "profiles/ProfileManager.hpp"
#include "profiles/ProfileLoader.hpp"
#include "esp_log.h"
#include <cstdio>

namespace InertialSaber::Profiles {

static constexpr const char *TAG = "ProfileManager";

void ProfileManager::init() {
  ESP_LOGI(TAG, "Initializing profiles...");

  esp_err_t err = ProfileLoader::loadFromSd(m_profiles);

  if (err != ESP_OK || m_profiles.empty()) {
    ESP_LOGE(TAG, "No profiles found on SD (err=%s, count=%u)",
             esp_err_to_name(err), (unsigned)m_profiles.size());
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
}

void ProfileManager::loadActive(Core::SaberActionBus &bus,
                               Espressif::Wrappers::Audio::AudioEngine &audio,
                               Espressif::Wrappers::SmartLed::Engine &led) {
  if (m_profiles.empty()) return;
  m_profiles[m_activeIndex]->load(bus, audio, led, *this, m_statusLed
#if CONFIG_IDF_TARGET_ESP32S3
                                  , m_psramCache
#endif
  );
}

void ProfileManager::nextProfile(Core::SaberActionBus &bus,
                                 Espressif::Wrappers::Audio::AudioEngine &audio,
                                 Espressif::Wrappers::SmartLed::Engine &led) {
  if (m_profiles.size() <= 1) return;

  ESP_LOGI(TAG, "Hot-swapping profile: unloading active index %u", m_activeIndex);
  m_profiles[m_activeIndex]->unload(bus
#if CONFIG_IDF_TARGET_ESP32S3
                                    , m_psramCache
#endif
  );

  m_activeIndex = (m_activeIndex + 1) % m_profiles.size();

  ESP_LOGI(TAG, "Loading next profile at index %u...", m_activeIndex);
  m_profiles[m_activeIndex]->load(bus, audio, led, *this, m_statusLed
#if CONFIG_IDF_TARGET_ESP32S3
                                  , m_psramCache
#endif
  );
  saveActiveIndex();
}

void ProfileManager::prevProfile(Core::SaberActionBus &bus,
                                 Espressif::Wrappers::Audio::AudioEngine &audio,
                                 Espressif::Wrappers::SmartLed::Engine &led) {
  if (m_profiles.size() <= 1) return;

  ESP_LOGI(TAG, "Hot-swapping profile: unloading active index %u", m_activeIndex);
  m_profiles[m_activeIndex]->unload(bus
#if CONFIG_IDF_TARGET_ESP32S3
                                    , m_psramCache
#endif
  );

  if (m_activeIndex == 0) {
    m_activeIndex = m_profiles.size() - 1;
  } else {
    m_activeIndex--;
  }

  ESP_LOGI(TAG, "Loading previous profile at index %u...", m_activeIndex);
  m_profiles[m_activeIndex]->load(bus, audio, led, *this, m_statusLed
#if CONFIG_IDF_TARGET_ESP32S3
                                  , m_psramCache
#endif
  );
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

ConfigurableProfile &ProfileManager::getActiveProfile() const {
  return *m_profiles[m_activeIndex];
}

size_t ProfileManager::getProfileCount() const {
  return m_profiles.size();
}

} // namespace InertialSaber::Profiles
