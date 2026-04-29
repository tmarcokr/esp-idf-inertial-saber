#include "SaberSystem.hpp"

#include "esp_log.h"

extern "C" void app_main(void) {
    static const char* TAG = "Main";

    static InertialSaber::SaberSystem system;

    if (system.start() != ESP_OK) {
        ESP_LOGE(TAG, "System startup failed");
        return;
    }
}
