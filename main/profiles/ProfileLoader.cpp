#include "profiles/ProfileLoader.hpp"
#include "esp_log.h"
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

namespace InertialSaber::Profiles {

static constexpr const char *TAG = "ProfileLoader";

namespace {

struct DirCloser {
  void operator()(DIR *dir) const { closedir(dir); }
};

struct FileCloser {
  void operator()(FILE *file) const { fclose(file); }
};

} // namespace

esp_err_t ProfileLoader::loadFromSd(std::vector<std::unique_ptr<ConfigurableProfile>> &profiles) {
  ESP_LOGI(TAG, "Scanning /sdcard/profiles/ ...");
  std::unique_ptr<DIR, DirCloser> dir(opendir("/sdcard/profiles"));
  if (!dir) {
    ESP_LOGE(TAG, "opendir('/sdcard/profiles') FAILED — directory not found");
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

    ESP_LOGI(TAG, "  entry: '%s' (d_type=%d)", entry->d_name, entry->d_type);

    std::string configPath = std::string("/sdcard/profiles/") + entry->d_name + "/profile.json";
    ESP_LOGI(TAG, "  trying: %s", configPath.c_str());

    std::unique_ptr<FILE, FileCloser> file(fopen(configPath.c_str(), "r"));
    if (!file) {
      ESP_LOGW(TAG, "  fopen FAILED for: %s", configPath.c_str());
      continue;
    }

    fseek(file.get(), 0, SEEK_END);
    long size = ftell(file.get());
    fseek(file.get(), 0, SEEK_SET);

    ESP_LOGI(TAG, "  file size: %ld bytes", size);

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
