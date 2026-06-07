#include "DragEffect.hpp"
#include "AudioEngine.hpp"
#include "../overlays/BladeDragEffect.hpp"
#include "Engine.hpp"
#include "PowerToggleEffect.hpp"
#include "models/InertialDefinition.hpp"
#include "models/SaberDataPacket.hpp"

#include "esp_log.h"
#include "esp_random.h"

#include <algorithm>

namespace InertialSaber::Effects {

static constexpr const char* TAG = "DragEffect";

DragEffect::DragEffect(
    PowerToggleEffect& power,
    Espressif::Wrappers::Audio::AudioEngine& audio,
    Espressif::Wrappers::SmartLed::Engine& ledEngine,
    const Core::InertialDefinition& definition,
    uint8_t buttonId)
    : m_power(power)
    , m_audio(audio)
    , m_ledEngine(ledEngine)
    , m_def(definition)
    , m_buttonId(buttonId) {
    Priority = 1;
}

bool DragEffect::Test(const Core::SaberDataPacket& packet) {
    if (!m_power.isIgnited()) {
        m_triggerMet = false;
        return m_active;
    }

    if (m_buttonId < Core::Platform::kMaxInputs) {
        const auto& input = packet.inputs[m_buttonId];
        using Gesture    = Core::InputDescriptor::Gesture;
        using InputState = Core::InputDescriptor::State;

        if (input.gesture == Gesture::HOLD_TICK && input.holdLevel == 1) {
            m_triggerMet = true;
        } else if (input.current == InputState::RELEASED && m_active) {
            m_triggerMet = false;
        }
    }

    return m_triggerMet || m_active;
}

void DragEffect::Run() {
    if (m_triggerMet && !m_active) {
        m_active = true;

        const uint8_t index = static_cast<uint8_t>(
            esp_random() % std::max<uint8_t>(m_def.fontDragCount, 1));
        const std::string path = buildPath("drag/drag", index);

        m_audioChannel = m_audio.play(path, true, 16384);

        auto overlay = std::make_unique<BladeDragEffect>(
            m_ledEngine.numLeds(), m_def.dragLedCount);
        m_ledEffect = overlay.get();

        if (!m_ledEngine.pushOverlay(std::move(overlay))) {
            m_ledEffect = nullptr;
        }

        ESP_LOGI(TAG, "Drag active: %s", path.c_str());
    } else if (!m_triggerMet && m_active) {
        m_active = false;

        if (m_audioChannel != Espressif::Wrappers::Audio::INVALID_CHANNEL) {
            m_audio.stop(m_audioChannel);
            m_audioChannel = Espressif::Wrappers::Audio::INVALID_CHANNEL;
        }

        const uint8_t endIdx = static_cast<uint8_t>(
            esp_random() % std::max<uint8_t>(m_def.fontDragEndCount, 1));
        const std::string endPath = buildPath("enddrag/enddrag", endIdx);
        m_audio.play(endPath, false, 16384);

        if (m_ledEffect != nullptr) {
            m_ledEffect->terminate();
            m_ledEffect = nullptr;
        }

        ESP_LOGI(TAG, "Drag inactive, playing end: %s", endPath.c_str());
    }
}

std::string DragEffect::buildPath(const char* subAndPrefix, uint8_t index) const {
    return std::string("/sdcard/") + m_def.profileRoot + subAndPrefix +
           std::to_string(index + 1) + ".wav";
}

} // namespace InertialSaber::Effects
