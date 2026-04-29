#include "SaberActionBus.hpp"

#include "esp_log.h"

#include <algorithm>
#include <cstring>

namespace InertialSaber::Core {

static constexpr const char* TAG = "SaberActionBus";

SaberActionBus::SaberActionBus() = default;

SaberActionBus::~SaberActionBus() {
    stop();
}

esp_err_t SaberActionBus::start() {
    if (m_running) {
        ESP_LOGW(TAG, "Bus already running");
        return ESP_FAIL;
    }

    m_inputQueue = xQueueCreate(Platform::kInputQueueDepth, sizeof(InputEvent));
    if (m_inputQueue == nullptr) {
        ESP_LOGE(TAG, "Failed to create input queue");
        return ESP_FAIL;
    }

    m_running = true;

    BaseType_t result = xTaskCreatePinnedToCore(
        busTaskEntry,
        "saber_bus",
        Platform::kBusTaskStackSize,
        this,
        Platform::kBusTaskPriority,
        &m_taskHandle,
        Platform::kBusTaskCore
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create bus task");
        m_running = false;
        vQueueDelete(m_inputQueue);
        m_inputQueue = nullptr;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Bus started on core %d (priority %d)",
             Platform::kBusTaskCore, Platform::kBusTaskPriority);
    return ESP_OK;
}

void SaberActionBus::stop() {
    if (!m_running) {
        return;
    }

    m_running = false;

    if (m_taskHandle != nullptr) {
        xTaskNotifyGive(m_taskHandle);
        // Without this delay, m_taskHandle could be nullified before the task reads m_running
        vTaskDelay(pdMS_TO_TICKS(Platform::kBusTimeoutMs * 2));
        m_taskHandle = nullptr;
    }

    if (m_inputQueue != nullptr) {
        vQueueDelete(m_inputQueue);
        m_inputQueue = nullptr;
    }

    ESP_LOGI(TAG, "Bus stopped");
}

void SaberActionBus::registerEffect(std::unique_ptr<InertialEffect> effect) {
    if (!effect) {
        return;
    }
    m_effects.push_back(std::move(effect));
    std::sort(m_effects.begin(), m_effects.end(),
              [](const auto& a, const auto& b) {
                  return a->Priority < b->Priority;
              });
}

void SaberActionBus::clearEffects() {
    m_effects.clear();
}

void SaberActionBus::updateMotion(float energy, const float rotation[3], float orientation) {
    m_stagedEnergy = energy;
    m_stagedRotation[0] = rotation[0];
    m_stagedRotation[1] = rotation[1];
    m_stagedRotation[2] = rotation[2];
    m_stagedOrientation = orientation;

    if (m_taskHandle != nullptr) {
        xTaskNotifyGive(m_taskHandle);
    }
}

void SaberActionBus::pushInputEvent(uint8_t inputId, const InputDescriptor& descriptor) {
    if (inputId >= Platform::kMaxInputs || m_inputQueue == nullptr) {
        return;
    }

    InputEvent event{inputId, descriptor};
    xQueueSend(m_inputQueue, &event, 0);

    if (m_taskHandle != nullptr) {
        xTaskNotifyGive(m_taskHandle);
    }
}

TaskHandle_t SaberActionBus::getTaskHandle() const {
    return m_taskHandle;
}

void SaberActionBus::busTaskEntry(void* arg) {
    auto* bus = static_cast<SaberActionBus*>(arg);
    bus->busLoop();
    vTaskDelete(nullptr);
}

void SaberActionBus::busLoop() {
    while (m_running) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(Platform::kBusTimeoutMs));

        if (!m_running) {
            break;
        }

        applyStagedMotion();
        drainInputQueue();
        m_packet.timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        for (auto& effect : m_effects) {
            if (effect->Test(m_packet)) {
                effect->Run();
            }
        }
    }

    ESP_LOGI(TAG, "Bus task exiting");
}

void SaberActionBus::drainInputQueue() {
    InputEvent event{};
    while (xQueueReceive(m_inputQueue, &event, 0) == pdTRUE) {
        if (event.inputId < Platform::kMaxInputs) {
            m_packet.inputs[event.inputId] = event.descriptor;
        }
    }
}

void SaberActionBus::applyStagedMotion() {
    m_packet.KineticEnergy = m_stagedEnergy;
    m_packet.AxisRotation[0] = m_stagedRotation[0];
    m_packet.AxisRotation[1] = m_stagedRotation[1];
    m_packet.AxisRotation[2] = m_stagedRotation[2];
    m_packet.OrientationVector = m_stagedOrientation;
}

} // namespace InertialSaber::Core
