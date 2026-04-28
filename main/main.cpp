#include "helloworld/HelloWorldSample.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief FreeRTOS task wrapper for ISample execution.
 */
static void sample_task(void* pvParameters) {
    Espressif::App::ISample* sample = static_cast<Espressif::App::ISample*>(pvParameters);
    if (sample) {
        sample->run();
    }
    vTaskDelete(NULL);
}

extern "C" void app_main(void) {
    ESP_LOGI("Main", "Starting up the application template...");

    // Instantiate the HelloWorld sample
    static Espressif::App::HelloWorldSample hello_sample;

    // Hardware Setup
    if (hello_sample.setup() != ESP_OK) {
        ESP_LOGE("Main", "HelloWorld Sample setup failed.");
    }

    // Launch the task
    xTaskCreate(sample_task, "hello_task", 4096, &hello_sample, 5, NULL);

    ESP_LOGI("Main", "HelloWorld sample task launched.");
}
