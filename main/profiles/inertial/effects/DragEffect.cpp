// SPDX-License-Identifier: GPL-3.0-or-later

#include "DragEffect.hpp"
#include "diagnostics/Metrics.hpp"
#include "AudioLevels.hpp"
#include "overlays/BladeDragEffect.hpp"
#include "Engine.hpp"
#include "profiles/inertial/InertialDefinition.hpp"
#include "profiles/PowerStateMachine.hpp"
#include "profiles/SoundFont.hpp"
#include "core/SaberDataPacket.hpp"

#include "esp_log.h"

namespace InertialSaber::Effects {

static constexpr const char* TAG = "DragEffect";

DragEffect::DragEffect(const Profiles::PowerStateMachine& power, System::AudioController& audio,
                       Espressif::Wrappers::SmartLed::Engine& ledEngine,
                       const InertialSaber::Profiles::Inertial::InertialDefinition& definition,
                       const Profiles::SoundFont& font, uint8_t buttonId)
    : InertialEffect(1)
    , m_power(power)
    , m_audio(audio)
    , m_ledEngine(ledEngine)
    , m_def(definition)
    , m_font(font)
    , m_buttonId(buttonId)
    , m_loop(audio.acquireVoice()) {
    if (!m_loop.valid()) {
        ESP_LOGE(TAG, "No audio voice for the drag loop; drag plays without its loop");
    }
}

bool DragEffect::test(const Core::SaberDataPacket& packet) {
    if (!m_power.isIgnited()) {
        m_triggerMet = false;
        return m_active;
    }

    if (m_buttonId < Core::kMaxInputs) {
        const auto& input = packet.inputs[m_buttonId];
        using Gesture = Core::InputDescriptor::Gesture;
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
    SABER_METRIC_SCOPE(Diagnostics::Metric::RunDrag);
    if (m_triggerMet && !m_active) {
        m_active = true;

        const System::AudioPath path = m_font.randomPath(Profiles::FontCategory::Drag);

        m_loop.play(path, true, kFullVolume);

        m_overlayFadeRequest = std::make_shared<std::atomic<bool>>(false);
        auto overlay = std::make_unique<BladeDragEffect>(m_ledEngine.numLeds(), m_def.dragLedCount,
                                                         m_overlayFadeRequest);

        if (!m_ledEngine.pushOverlay(std::move(overlay))) {
            SABER_METRIC_COUNT(Diagnostics::Counter::OverlaysDropped);
            m_overlayFadeRequest.reset();
        }

        ESP_LOGD(TAG, "Drag active: %s", path.c_str());
    } else if (!m_triggerMet && m_active) {
        m_active = false;

        m_loop.stop();

        const System::AudioPath endPath = m_font.randomPath(Profiles::FontCategory::DragEnd);
        m_audio.playOneShot(endPath, kFullVolume);

        if (m_overlayFadeRequest) {
            m_overlayFadeRequest->store(true);
            m_overlayFadeRequest.reset();
        }

        ESP_LOGD(TAG, "Drag inactive, playing end: %s", endPath.c_str());
    }
}

} // namespace InertialSaber::Effects
