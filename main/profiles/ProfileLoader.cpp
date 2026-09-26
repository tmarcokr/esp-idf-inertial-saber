#include "profiles/ProfileLoader.hpp"
#include "system/Raii.hpp"
#include "esp_log.h"
#include <dirent.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace InertialSaber::Profiles {

static constexpr const char *TAG = "ProfileLoader";
static constexpr const char *kProfilesDir = "/sdcard/profiles";

esp_err_t ProfileLoader::loadFromSd(std::vector<std::unique_ptr<ConfigurableProfile>> &profiles) {
  ESP_LOGI(TAG, "Scanning %s/ ...", kProfilesDir);
  System::UniqueDir dir = System::openDir(kProfilesDir);
  if (!dir) {
    ESP_LOGE(TAG, "opendir('%s') FAILED — directory not found", kProfilesDir);
    return ESP_ERR_NOT_FOUND;
  }

  struct dirent *entry;
  int entryCount = 0;
  while ((entry = readdir(dir.get())) != nullptr) {
    entryCount++;
    if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
      ESP_LOGD(TAG, "  skip: '%s'", entry->d_name);
      continue;
    }

    ESP_LOGD(TAG, "  entry: '%s' (d_type=%d)", entry->d_name, entry->d_type);

    std::string configPath = std::string(kProfilesDir) + "/" + entry->d_name + "/profile.json";
    ESP_LOGD(TAG, "  trying: %s", configPath.c_str());

    System::UniqueFile file = System::openFile(configPath.c_str(), "r");
    if (!file) {
      ESP_LOGW(TAG, "  fopen FAILED for: %s", configPath.c_str());
      continue;
    }

    long size = -1;
    if (fseek(file.get(), 0, SEEK_END) == 0) {
      size = ftell(file.get());
    }
    if (size < 0 || fseek(file.get(), 0, SEEK_SET) != 0) {
      ESP_LOGW(TAG, "  cannot determine size of: %s", configPath.c_str());
      continue;
    }

    ESP_LOGD(TAG, "  file size: %ld bytes", size);

    std::string jsonStr;
    if (size > 0) {
      jsonStr.resize(size);
      size_t readBytes = fread(jsonStr.data(), 1, size, file.get());
      jsonStr.resize(readBytes);
    }
    file.reset();

    if (auto profile = ConfigurableProfile::fromJson(jsonStr)) {
      ESP_LOGI(TAG, "  LOADED profile '%s' from SD card", profile->definition().profileName.c_str());
      profiles.push_back(std::move(profile));
    } else {
      ESP_LOGE(TAG, "  PARSE FAILED for: %s", configPath.c_str());
    }
  }

  dir.reset();
  ESP_LOGI(TAG, "Scan complete: %d entries seen, %u profiles loaded", entryCount, (unsigned)profiles.size());
  return ESP_OK;
}

} // namespace InertialSaber::Profiles
