#include "profiles/ProfileLoader.hpp"
#include "profiles/ProfileParser.hpp"
#include "esp_log.h"
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>

namespace InertialSaber::Profiles {

static constexpr const char *TAG = "ProfileLoader";

esp_err_t ProfileLoader::loadFromSd(std::vector<std::unique_ptr<ConfigurableProfile>> &profiles) {
  ESP_LOGI(TAG, "Scanning /sdcard/profiles/ ...");
  DIR *dir = opendir("/sdcard/profiles");
  if (!dir) {
    ESP_LOGE(TAG, "opendir('/sdcard/profiles') FAILED — directory not found");
    return ESP_ERR_NOT_FOUND;
  }

  struct dirent *entry;
  int entryCount = 0;
  while ((entry = readdir(dir)) != nullptr) {
    entryCount++;
    if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
      ESP_LOGD(TAG, "  skip: '%s'", entry->d_name);
      continue;
    }

    ESP_LOGI(TAG, "  entry: '%s' (d_type=%d)", entry->d_name, entry->d_type);

    std::string configPath = std::string("/sdcard/profiles/") + entry->d_name + "/profile.json";
    ESP_LOGI(TAG, "  trying: %s", configPath.c_str());

    FILE *f = fopen(configPath.c_str(), "r");
    if (!f) {
      ESP_LOGW(TAG, "  fopen FAILED for: %s", configPath.c_str());
      continue;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    ESP_LOGI(TAG, "  file size: %ld bytes", size);

    std::string jsonStr;
    if (size > 0) {
      jsonStr.resize(size);
      size_t readBytes = fread(&jsonStr[0], 1, size, f);
      jsonStr.resize(readBytes);
    }
    fclose(f);

    InertialSaber::Profiles::Inertial::InertialDefinition tempDef{};
    std::string tempName;
    std::string tempRoot;
    if (ProfileParser::parse(jsonStr.c_str(), tempDef, tempName, tempRoot) == ESP_OK) {
      ESP_LOGI(TAG, "  LOADED profile '%s' from SD card", tempDef.profileName);
      profiles.push_back(std::make_unique<ConfigurableProfile>(jsonStr));
    } else {
      ESP_LOGE(TAG, "  PARSE FAILED for: %s", configPath.c_str());
    }
  }

  closedir(dir);
  ESP_LOGI(TAG, "Scan complete: %d entries seen, %u profiles loaded", entryCount, (unsigned)profiles.size());
  return ESP_OK;
}

} // namespace InertialSaber::Profiles
