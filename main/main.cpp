#include "system/SaberSystem.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/task.h"

extern "C" void app_main(void) {
    static const char* TAG = "Main";

    vTaskDelay(pdMS_TO_TICKS(1000));

    static InertialSaber::System::SaberSystem system;

    if (system.start() != ESP_OK) {
        ESP_LOGE(TAG, "System startup failed");
        return;
    }
}
