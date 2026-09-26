#include "DragEffect.hpp"
#include "AudioEngine.hpp"
#include "AudioLevels.hpp"
#include "overlays/BladeDragEffect.hpp"
#include "Engine.hpp"
#include "PowerToggleEffect.hpp"
#include "profiles/inertial/InertialDefinition.hpp"
#include "profiles/SoundFont.hpp"
#include "core/SaberDataPacket.hpp"

#include "esp_log.h"

#include <string>

namespace InertialSaber::Effects {

static constexpr const char* TAG = "DragEffect";

DragEffect::DragEffect(
    PowerToggleEffect& power,
    Espressif::Wrappers::Audio::AudioEngine& audio,
    Espressif::Wrappers::SmartLed::Engine& ledEngine,
    const InertialSaber::Profiles::Inertial::InertialDefinition& definition,
    const Profiles::SoundFont& font,
    uint8_t buttonId)
    : InertialEffect(1)
    , m_power(power)
    , m_audio(audio)
    , m_ledEngine(ledEngine)
    , m_def(definition)
    , m_font(font)
    , m_buttonId(buttonId) {}

bool DragEffect::test(const Core::SaberDataPacket& packet) {
    if (!m_power.isIgnited()) {
        m_triggerMet = false;
        return m_active;
    }

    if (m_buttonId < Core::kMaxInputs) {
        const auto& input = packet.inputs[m_buttonId];
        using Gesture    = Core::InputDescriptor::Gesture;
        using InputState = Core::InputDescriptor::State;

        if (input.gesture == Gesture::HoldTick && input.holdLevel == 1) {
            m_triggerMet = true;
        } else if (input.current == InputState::Released && m_active) {
            m_triggerMet = false;
        }
    }

    return m_triggerMet || m_active;
}

void DragEffect::run() {
    if (m_triggerMet && !m_active) {
        m_active = true;

        const std::string path = m_font.randomPath(Profiles::FontCategory::Drag);

        m_audioChannel = m_audio.play(path, true, kFullVolume);

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

        const std::string endPath = m_font.randomPath(Profiles::FontCategory::DragEnd);
        m_audio.play(endPath, false, kFullVolume);

        if (m_ledEffect != nullptr) {
            m_ledEffect->terminate();
            m_ledEffect = nullptr;
        }

        ESP_LOGI(TAG, "Drag inactive, playing end: %s", endPath.c_str());
    }
}

} // namespace InertialSaber::Effects
