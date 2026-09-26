#include "SaberActionBus.hpp"

#include "esp_log.h"
#include "esp_timer.h"

#include <algorithm>
#include <cstring>

namespace InertialSaber::Core {

static constexpr const char* TAG = "SaberActionBus";

SaberActionBus::SaberActionBus(const BusConfig& config) : m_config(config) {}

SaberActionBus::~SaberActionBus() {
    stop();
}

esp_err_t SaberActionBus::start() {
    if (m_running) {
        ESP_LOGW(TAG, "Bus already running");
        return ESP_FAIL;
    }

    QueueHandle_t queue = xQueueCreate(kInputQueueDepth, sizeof(InputEvent));
    if (queue == nullptr) {
        ESP_LOGE(TAG, "Failed to create input queue");
        return ESP_FAIL;
    }
    m_inputQueue = queue;

    m_running = true;
    m_lastLoopTimeUs = esp_timer_get_time();

    TaskHandle_t handle = nullptr;
    BaseType_t result = xTaskCreatePinnedToCore(
        busTaskEntry,
        "saber_bus",
        m_config.task.stackSize,
        this,
        m_config.task.priority,
        &handle,
        m_config.task.core
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create bus task");
        m_running = false;
        vQueueDelete(m_inputQueue.exchange(nullptr));
        return ESP_FAIL;
    }
    m_taskHandle = handle;

    ESP_LOGI(TAG, "Bus started on core %d (priority %d)",
             static_cast<int>(m_config.task.core),
             static_cast<int>(m_config.task.priority));
    return ESP_OK;
}

void SaberActionBus::stop() {
    if (!m_running) {
        return;
    }

    m_running = false;

    if (TaskHandle_t handle = m_taskHandle.exchange(nullptr); handle != nullptr) {
        xTaskNotifyGive(handle);
        // Warning: there is no join; the delay lets the bus task exit before its queue is deleted.
        vTaskDelay(pdMS_TO_TICKS(kBusTimeoutMs * 2));
    }

    if (QueueHandle_t queue = m_inputQueue.exchange(nullptr); queue != nullptr) {
        vQueueDelete(queue);
    }

    ESP_LOGI(TAG, "Bus stopped");
}

void SaberActionBus::setPhysicsConfig(const Core::PhysicsConfig& def) {
    m_kineticEnergyDeadbandG = def.kineticEnergyDeadbandG;
    m_rotationDeadbandDps    = def.rotationDeadbandDps;
    m_overloadThresholdG     = def.overloadThresholdG;
    m_overloadChargeRate     = def.overloadChargeRate;
    m_overloadDrainRate      = def.overloadDrainRate;
    m_burstCooldownMs        = def.burstCooldownMs;
}

void SaberActionBus::registerEffect(std::unique_ptr<InertialEffect> effect) {
    if (!effect) {
        return;
    }
    m_effects.push_back(std::move(effect));
    std::sort(m_effects.begin(), m_effects.end(),
              [](const auto& a, const auto& b) {
                  return a->priority() < b->priority();
              });
    m_effectsChanged = true;
}

void SaberActionBus::clearEffects() {
    for (auto& fx : m_effects) {
        m_effectsPendingDestruction.push_back(std::move(fx));
    }
    m_effects.clear();
    m_effectsChanged = true;
}

void SaberActionBus::updateMotion(const MotionSample& sample) {
    portENTER_CRITICAL(&m_motionLock);
    m_stagedMotion = sample;
    portEXIT_CRITICAL(&m_motionLock);

    if (TaskHandle_t handle = m_taskHandle; handle != nullptr) {
        xTaskNotifyGive(handle);
    }
}

void SaberActionBus::pushInputEvent(uint8_t inputId, const InputDescriptor& descriptor) {
    QueueHandle_t queue = m_inputQueue;
    if (inputId >= kMaxInputs || queue == nullptr) {
        return;
    }

    InputEvent event{inputId, descriptor};
    xQueueSend(queue, &event, 0);

    if (TaskHandle_t handle = m_taskHandle; handle != nullptr) {
        xTaskNotifyGive(handle);
    }
}

void SaberActionBus::busTaskEntry(void* arg) {
    auto* bus = static_cast<SaberActionBus*>(arg);
    bus->busLoop();
    vTaskDelete(nullptr);
}

void SaberActionBus::busLoop() {
    while (m_running) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kBusTimeoutMs));

        if (!m_running) {
            break;
        }

        m_packet.timestampMs = xTaskGetTickCount() * portTICK_PERIOD_MS;

        applyStagedMotion();
        computeInertialOverload();
        drainInputQueue();

        m_effectsChanged = false;
        std::vector<InertialEffect*> activeEffects;
        activeEffects.reserve(m_effects.size());
        for (const auto& fx : m_effects) {
            activeEffects.push_back(fx.get());
        }

        for (auto* effect : activeEffects) {
            if (m_effectsChanged) {
                break;
            }
            if (effect->test(m_packet)) {
                effect->run();
            }
        }

        m_effectsPendingDestruction.clear();
        m_packet.inputs = {};
    }

    ESP_LOGI(TAG, "Bus task exiting");
}

void SaberActionBus::drainInputQueue() {
    QueueHandle_t queue = m_inputQueue;
    InputEvent event{};
    while (xQueueReceive(queue, &event, 0) == pdTRUE) {
        if (event.inputId < kMaxInputs) {
            m_packet.inputs[event.inputId] = event.descriptor;
        }
    }
}

void SaberActionBus::loadStagedMotionToPacket() {
    portENTER_CRITICAL(&m_motionLock);
    const MotionSample sample = m_stagedMotion;
    portEXIT_CRITICAL(&m_motionLock);

    m_packet.kineticEnergy = sample.kineticEnergyG;
    m_packet.axisRotation = sample.axisRotationDps;
    m_packet.orientation = sample.orientationDeg;
}

void SaberActionBus::filterStagedMotionWarmUp() {
    if (m_packet.timestampMs < m_config.motion.warmUpPeriodMs) {
        m_packet.kineticEnergy = 0.0f;
        m_packet.axisRotation[0] = 0.0f;
        m_packet.axisRotation[1] = 0.0f;
        m_packet.axisRotation[2] = 0.0f;
        m_packet.orientation = 90.0f + m_config.motion.orientationOffsetDeg;
    }
}

void SaberActionBus::filterStagedMotionStabilization() {
    if (m_packet.kineticEnergy < m_kineticEnergyDeadbandG) {
        m_packet.kineticEnergy = 0.0f;
    }

    for (int i = 0; i < 3; ++i) {
        if (std::abs(m_packet.axisRotation[i]) < m_rotationDeadbandDps) {
            m_packet.axisRotation[i] = 0.0f;
        }
    }
}

void SaberActionBus::filterStagedMotionOrientation() {
    float correctedAngle = m_packet.orientation - m_config.motion.orientationOffsetDeg;
    if (correctedAngle > 90.0f) correctedAngle = 90.0f;
    if (correctedAngle < -90.0f) correctedAngle = -90.0f;
    
    m_packet.orientation = correctedAngle / 90.0f;
}

void SaberActionBus::applyStagedMotion() {
    loadStagedMotionToPacket();
    filterStagedMotionWarmUp();
    filterStagedMotionStabilization();
    filterStagedMotionOrientation();
}

void SaberActionBus::computeInertialOverload() {
    float dtSec = calculateDeltaTimeSec();

    if (isInertialOverloadInCooldown()) {
        resetInertialOverloadState();
        return;
    }

    chargeOrDrainInertialOverload(dtSec);
    clampInertialOverloadLevel();
    evaluateInertialBurst();
}

float SaberActionBus::calculateDeltaTimeSec() {
    int64_t nowUs = esp_timer_get_time();
    float dtSec = static_cast<float>(nowUs - m_lastLoopTimeUs) / 1000000.0f;
    m_lastLoopTimeUs = nowUs;
    return dtSec;
}

bool SaberActionBus::isInertialOverloadInCooldown() const {
    return (m_packet.timestampMs - m_lastBurstTimeMs) < m_burstCooldownMs;
}

void SaberActionBus::resetInertialOverloadState() {
    m_overloadLevel = 0.0f;
    m_packet.inertialOverload = 0.0f;
    m_packet.inertialBurst = false;
}

void SaberActionBus::chargeOrDrainInertialOverload(float dtSec) {
    if (m_packet.kineticEnergy > m_overloadThresholdG) {
        m_overloadLevel += m_overloadChargeRate * dtSec;
    } else {
        m_overloadLevel -= m_overloadDrainRate * dtSec;
    }
}

void SaberActionBus::clampInertialOverloadLevel() {
    if (m_overloadLevel < 0.0f) {
        m_overloadLevel = 0.0f;
    } else if (m_overloadLevel > 1.0f) {
        m_overloadLevel = 1.0f;
    }
}

void SaberActionBus::evaluateInertialBurst() {
    m_packet.inertialBurst = false;
    if (m_overloadLevel >= 1.0f) {
        m_packet.inertialBurst = true;
        m_lastBurstTimeMs = m_packet.timestampMs;
        m_overloadLevel = 0.0f;
    }
    m_packet.inertialOverload = m_overloadLevel;
}

} // namespace InertialSaber::Core
