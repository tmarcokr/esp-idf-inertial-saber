#include "system/persistence/ActiveProfileStore.hpp"
#include "system/Raii.hpp"
#include "diagnostics/Metrics.hpp"

#include "esp_log.h"

#include <cinttypes>
#include <cstdio>
#include <utility>

namespace InertialSaber::System {

namespace {

constexpr const char* TAG = "ActiveProfileStore";
constexpr const char* kActiveProfilePath = "/sdcard/active_profile.txt";

} // namespace

ActiveProfileStore::ActiveProfileStore(const Hardware::TaskSpec& task)
    : m_taskSpec(task)
    , m_queue(xQueueCreateStatic(1, sizeof(uint32_t), m_queueStorage.data(), &m_queueControl))
    , m_exitSemaphore(xSemaphoreCreateBinaryStatic(&m_exitSemaphoreControl)) {}

ActiveProfileStore::~ActiveProfileStore() {
    if (m_task != nullptr) {
        configASSERT(xTaskGetCurrentTaskHandle() != m_task);
        const uint32_t shutdown = kShutdownRequest;
        xQueueSend(m_queue, &shutdown, portMAX_DELAY);
        xSemaphoreTake(m_exitSemaphore, portMAX_DELAY);
        m_task = nullptr;
    }
    vSemaphoreDelete(m_exitSemaphore);
    vQueueDelete(m_queue);
}

esp_err_t ActiveProfileStore::start() {
    if (m_task != nullptr) return ESP_ERR_INVALID_STATE;

    if (xTaskCreatePinnedToCore(&ActiveProfileStore::taskEntry, m_taskSpec.name,
                                m_taskSpec.stackSize, this, m_taskSpec.priority, &m_task,
                                m_taskSpec.core) != pdPASS) {
        m_task = nullptr;
        ESP_LOGE(TAG, "%s task creation failed", m_taskSpec.name);
        return ESP_ERR_NO_MEM;
    }
    SABER_METRIC_REGISTER_TASK(Diagnostics::TaskId::ProfileStore, m_task);
    return ESP_OK;
}

std::optional<size_t> ActiveProfileStore::load() const {
    UniqueFile file = openFile(kActiveProfilePath, "r");
    if (!file) {
        ESP_LOGW(TAG, "active_profile.txt not found, defaulting to index 0");
        return std::nullopt;
    }
    uint32_t index = 0;
    if (fscanf(file.get(), "%" SCNu32, &index) != 1) {
        ESP_LOGW(TAG, "Failed to parse active_profile.txt content");
        return std::nullopt;
    }
    return index;
}

void ActiveProfileStore::saveAsync(size_t index) {
    if (index >= kShutdownRequest) return;
    const auto value = static_cast<uint32_t>(index);
    xQueueOverwrite(m_queue, &value);
}

void ActiveProfileStore::taskEntry(void* arg) {
    static_cast<ActiveProfileStore*>(arg)->run();
}

void ActiveProfileStore::run() {
    uint32_t index = 0;
    while (xQueueReceive(m_queue, &index, portMAX_DELAY) == pdTRUE && index != kShutdownRequest) {
        write(index);
    }
    xSemaphoreGive(m_exitSemaphore);
    vTaskDelete(nullptr);
}

void ActiveProfileStore::write(uint32_t index) const {
    UniqueFile file = openFile(kActiveProfilePath, "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open active_profile.txt for writing");
        return;
    }
    const bool written = fprintf(file.get(), "%" PRIu32 "\n", index) > 0;
    const bool closed = closeFile(std::move(file));
    if (!written || !closed) {
        ESP_LOGE(TAG, "Failed to write active_profile.txt");
        return;
    }
    ESP_LOGI(TAG, "Saved active profile index: %" PRIu32, index);
}

} // namespace InertialSaber::System
