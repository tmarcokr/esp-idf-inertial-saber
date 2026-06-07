#include "profiles/ProfileManager.hpp"
#include "profiles/ProfileLoader.hpp"
#include "profiles/inertial/effects/ProfileCycleEffect.hpp"
#include "system/config/HardwareConfig.hpp"
#include "esp_log.h"
#include <cstdio>

namespace InertialSaber::Profiles {

static constexpr const char *TAG = "ProfileManager";

void ProfileManager::init() {
  ESP_LOGI(TAG, "Initializing profiles...");

  esp_err_t err = ProfileLoader::loadFromSd(m_profiles);

  if (err != ESP_OK || m_profiles.empty()) {
    ESP_LOGI(TAG, "No profiles found on SD. System will start without active profiles.");
  }

  m_activeIndex = 0;
  FILE *f = fopen("/sdcard/active_profile.txt", "r");
  if (f) {
    unsigned int loadedIndex = 0;
    if (fscanf(f, "%u", &loadedIndex) == 1) {
      if (loadedIndex < m_profiles.size()) {
        m_activeIndex = loadedIndex;
      } else {
        ESP_LOGW(TAG, "Loaded active index %u out of bounds (%u profiles). Resetting to 0.", loadedIndex, m_profiles.size());
      }
    }
    fclose(f);
  }
  ESP_LOGI(TAG, "Initialized %u profile(s)", m_profiles.size());
}

void ProfileManager::loadActive(Core::SaberActionBus &bus,
                               Espressif::Wrappers::Audio::AudioEngine &audio,
                               Espressif::Wrappers::SmartLed::Engine &led) {
  if (m_profiles.empty()) return;
  m_profiles[m_activeIndex]->load(bus, audio, led);
  registerSystemEffects(bus, audio, led);
}

void ProfileManager::nextProfile(Core::SaberActionBus &bus,
                                 Espressif::Wrappers::Audio::AudioEngine &audio,
                                 Espressif::Wrappers::SmartLed::Engine &led) {
  if (m_profiles.size() <= 1) return;

  ESP_LOGI(TAG, "Hot-swapping profile: unloading active index %u", m_activeIndex);
  m_profiles[m_activeIndex]->unload(bus);

  m_activeIndex = (m_activeIndex + 1) % m_profiles.size();

  ESP_LOGI(TAG, "Loading next profile at index %u...", m_activeIndex);
  m_profiles[m_activeIndex]->load(bus, audio, led);
  registerSystemEffects(bus, audio, led);
  saveActiveIndex();
}

void ProfileManager::prevProfile(Core::SaberActionBus &bus,
                                 Espressif::Wrappers::Audio::AudioEngine &audio,
                                 Espressif::Wrappers::SmartLed::Engine &led) {
  if (m_profiles.size() <= 1) return;

  ESP_LOGI(TAG, "Hot-swapping profile: unloading active index %u", m_activeIndex);
  m_profiles[m_activeIndex]->unload(bus);

  if (m_activeIndex == 0) {
    m_activeIndex = m_profiles.size() - 1;
  } else {
    m_activeIndex--;
  }

  ESP_LOGI(TAG, "Loading previous profile at index %u...", m_activeIndex);
  m_profiles[m_activeIndex]->load(bus, audio, led);
  registerSystemEffects(bus, audio, led);
  saveActiveIndex();
}

void ProfileManager::saveActiveIndex() {
  FILE *f = fopen("/sdcard/active_profile.txt", "w");
  if (f) {
    fprintf(f, "%u\n", (unsigned int)m_activeIndex);
    fclose(f);
  } else {
    ESP_LOGE(TAG, "Failed to open active_profile.txt for writing");
  }
}

void ProfileManager::registerSystemEffects(Core::SaberActionBus &bus,
                                           Espressif::Wrappers::Audio::AudioEngine &audio,
                                           Espressif::Wrappers::SmartLed::Engine &led) {
  bus.registerEffect(std::make_unique<Effects::ProfileCycleEffect>(
      *m_profiles[m_activeIndex],
      *this,
      bus,
      audio,
      led,
      System::Config::HardwareConfig::kMainBtnInputId));
}

Core::InertialProfile &ProfileManager::getActiveProfile() const {
  return *m_profiles[m_activeIndex];
}

size_t ProfileManager::getProfileCount() const {
  return m_profiles.size();
}

} // namespace InertialSaber::Profiles
