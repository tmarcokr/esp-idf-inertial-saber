#include "SaberActionBus.hpp"

#include "esp_log.h"
#include "esp_timer.h"

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

    m_inputQueue = xQueueCreate(kInputQueueDepth, sizeof(InputEvent));
    if (m_inputQueue == nullptr) {
        ESP_LOGE(TAG, "Failed to create input queue");
        return ESP_FAIL;
    }

    m_running = true;
    m_lastLoopTimeUs = esp_timer_get_time();

    BaseType_t result = xTaskCreatePinnedToCore(
        busTaskEntry,
        "saber_bus",
        System::Hardware::HardwareConfig::kBusTaskStackSize,
        this,
        System::Hardware::HardwareConfig::kBusTaskPriority,
        &m_taskHandle,
        System::Hardware::HardwareConfig::kBusTaskCore
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create bus task");
        m_running = false;
        vQueueDelete(m_inputQueue);
        m_inputQueue = nullptr;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Bus started on core %d (priority %d)",
             System::Hardware::HardwareConfig::kBusTaskCore,
             System::Hardware::HardwareConfig::kBusTaskPriority);
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
        vTaskDelay(pdMS_TO_TICKS(kBusTimeoutMs * 2));
        m_taskHandle = nullptr;
    }

    if (m_inputQueue != nullptr) {
        vQueueDelete(m_inputQueue);
        m_inputQueue = nullptr;
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
    if (inputId >= System::Hardware::HardwareConfig::kMaxInputs || m_inputQueue == nullptr) {
        return;
    }

    InputEvent event{inputId, descriptor};
    xQueueSend(m_inputQueue, &event, 0);

    if (m_taskHandle != nullptr) {
        xTaskNotifyGive(m_taskHandle);
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
    InputEvent event{};
    while (xQueueReceive(m_inputQueue, &event, 0) == pdTRUE) {
        if (event.inputId < System::Hardware::HardwareConfig::kMaxInputs) {
            m_packet.inputs[event.inputId] = event.descriptor;
        }
    }
}

void SaberActionBus::loadStagedMotionToPacket() {
    m_packet.kineticEnergy = m_stagedEnergy;
    m_packet.axisRotation[0] = m_stagedRotation[0];
    m_packet.axisRotation[1] = m_stagedRotation[1];
    m_packet.axisRotation[2] = m_stagedRotation[2];
    m_packet.orientation = m_stagedOrientation;
}

void SaberActionBus::filterStagedMotionWarmUp() {
    if (m_packet.timestampMs < System::Hardware::HardwareConfig::kImuGracePeriodMs) {
        m_packet.kineticEnergy = 0.0f;
        m_packet.axisRotation[0] = 0.0f;
        m_packet.axisRotation[1] = 0.0f;
        m_packet.axisRotation[2] = 0.0f;
        m_packet.orientation = 90.0f + System::Hardware::HardwareConfig::kImuOrientationOffsetDeg;
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
    float correctedAngle = m_packet.orientation - System::Hardware::HardwareConfig::kImuOrientationOffsetDeg;
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
