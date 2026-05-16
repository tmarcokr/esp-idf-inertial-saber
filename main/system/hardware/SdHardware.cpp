#include "SdHardware.hpp"
#include "system/config/HardwareConfig.hpp"
#include "esp_log.h"

namespace InertialSaber::System::Hardware {
static constexpr const char* TAG = "SdHardware";

esp_err_t SdHardware::init() {
    Espressif::Wrappers::SdCard::Config sd_cfg = {
        .miso = Config::HardwareConfig::kSdMiso,
        .mosi = Config::HardwareConfig::kSdMosi,
        .sck = Config::HardwareConfig::kSdSck,
        .cs = Config::HardwareConfig::kSdCs,
        .mount_point = "/sdcard",
        .max_files = 5,
        .format_if_mount_failed = false};

    m_sdCard = std::make_unique<Espressif::Wrappers::SdCard>(sd_cfg);
    esp_err_t err = m_sdCard->init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SD Card init failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "SD Card ready");
    return ESP_OK;
}
}
