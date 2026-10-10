// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/InertialEffect.hpp"
#include "system/audio/AudioController.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace InertialSaber::Profiles {
class PowerStateMachine;
class SoundFont;
} // namespace InertialSaber::Profiles
namespace InertialSaber::Profiles::Inertial {
struct InertialDefinition;
}
namespace InertialSaber::Core {
struct SaberDataPacket;
}
namespace Espressif::Wrappers::SmartLed {
class Engine;
}

namespace InertialSaber::Effects {

/**
 * @brief Evaluates blade drag trigger conditions and manages looping sound and LED overlay.
 */
class DragEffect final : public Core::InertialEffect {
public:
    /** @brief Audio voices acquired per instance (drag loop). */
    static constexpr size_t kVoiceCount = 1;

    /**
     * @brief Construct a new Drag Effect.
     * @param power Power state machine of the active profile.
     * @param audio Audio controller; one persistent voice is acquired for the drag loop.
     * @param ledEngine SmartLed engine reference.
     * @param definition Active inertial definition.
     * @param font Sound font of the active profile.
     * @param buttonId Trigger button identifier.
     */
    DragEffect(const Profiles::PowerStateMachine& power, System::AudioController& audio,
               Espressif::Wrappers::SmartLed::Engine& ledEngine,
               const Profiles::Inertial::InertialDefinition& definition,
               const Profiles::SoundFont& font, uint8_t buttonId);

    bool test(const Core::SaberDataPacket& packet) override;
    void run() override;

private:
    const Profiles::PowerStateMachine& m_power;
    System::AudioController& m_audio;
    Espressif::Wrappers::SmartLed::Engine& m_ledEngine;
    const Profiles::Inertial::InertialDefinition& m_def;
    const Profiles::SoundFont& m_font;
    uint8_t m_buttonId;

    System::AudioVoice m_loop;
    std::shared_ptr<std::atomic<bool>> m_overlayFadeRequest;
    bool m_active = false;
    bool m_triggerMet = false;
};

} // namespace InertialSaber::Effects
