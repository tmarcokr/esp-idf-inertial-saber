#include "SdHardware.hpp"
#include "system/hardware/HardwareConfig.hpp"
#include "esp_log.h"

namespace InertialSaber::System::Hardware {
static constexpr const char* TAG = "SdHardware";

esp_err_t SdHardware::init() {
    Espressif::Wrappers::SdCard::Config sd_cfg = {};
    sd_cfg.mount_point = "/sdcard";
    sd_cfg.max_files = 16;
    sd_cfg.format_if_mount_failed = false;

#if CONFIG_IDF_TARGET_ESP32S3
    sd_cfg.mode = Espressif::Wrappers::SdCard::HostMode::SDMMC_1BIT;
    sd_cfg.clk = Hardware::HardwareConfig::kSdClk;
    sd_cfg.cmd = Hardware::HardwareConfig::kSdCmd;
    sd_cfg.d0 = Hardware::HardwareConfig::kSdD0;
#else
    sd_cfg.mode = Espressif::Wrappers::SdCard::HostMode::SPI;
    sd_cfg.miso = Hardware::HardwareConfig::kSdMiso;
    sd_cfg.mosi = Hardware::HardwareConfig::kSdMosi;
    sd_cfg.sck = Hardware::HardwareConfig::kSdSck;
    sd_cfg.cs = Hardware::HardwareConfig::kSdCs;
#endif

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
