#include "HelloWorldSample.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace Espressif::App {

static const char* TAG = "HelloWorldSample";

esp_err_t HelloWorldSample::setup() {
    ESP_LOGI(TAG, "Setup: Hello World Sample is ready.");
    return ESP_OK;
}

void HelloWorldSample::run() {
    while (true) {
        ESP_LOGI(TAG, "Hello, World from ESP32!");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

} // namespace Espressif::App
