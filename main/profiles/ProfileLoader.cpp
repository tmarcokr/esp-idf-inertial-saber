#include "profiles/ProfileLoader.hpp"
#include "profiles/ConfigurableProfile.hpp"
#include "profiles/ProfileParser.hpp"
#include "esp_log.h"
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>

namespace InertialSaber::Profiles {

static constexpr const char *TAG = "ProfileLoader";

esp_err_t ProfileLoader::loadFromSd(std::vector<std::unique_ptr<Core::InertialProfile>> &profiles) {
  DIR *dir = opendir("/sdcard/profiles");
  if (!dir) {
    ESP_LOGW(TAG, "Profiles directory not found on SD card");
    return ESP_ERR_NOT_FOUND;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    std::string configPath = std::string("/sdcard/profiles/") + entry->d_name + "/profile.json";
    FILE *f = fopen(configPath.c_str(), "r");
    if (!f) {
      continue;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::string jsonStr;
    if (size > 0) {
      jsonStr.resize(size);
      size_t readBytes = fread(&jsonStr[0], 1, size, f);
      jsonStr.resize(readBytes);
    }
    fclose(f);

    Core::InertialDefinition tempDef{};
    std::string tempName;
    std::string tempRoot;
    if (ProfileParser::parse(jsonStr.c_str(), tempDef, tempName, tempRoot) == ESP_OK) {
      ESP_LOGI(TAG, "Discovered and loaded profile '%s' from SD card", tempDef.profileName);
      profiles.push_back(std::make_unique<ConfigurableProfile>(jsonStr));
    } else {
      ESP_LOGE(TAG, "Failed to parse profile config at %s", configPath.c_str());
    }
  }

  closedir(dir);
  return ESP_OK;
}

} // namespace InertialSaber::Profiles
