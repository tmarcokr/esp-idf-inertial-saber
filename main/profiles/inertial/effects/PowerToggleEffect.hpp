// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/InertialEffect.hpp"
#include <cstdint>

namespace InertialSaber::Core {
struct SaberDataPacket;
}
namespace InertialSaber::Profiles {
class PowerStateMachine;
class SoundFont;
} // namespace InertialSaber::Profiles
namespace InertialSaber::Profiles::Inertial {
struct InertialDefinition;
}
namespace InertialSaber::Effects {
class InertialSwingEffect;
class InertialLightEffect;
} // namespace InertialSaber::Effects
namespace InertialSaber::System {
class AudioController;
}
namespace Espressif::Wrappers::SmartLed {
class Engine;
}

namespace InertialSaber::Effects {

/**
 * @brief Sequenced ignition and retraction effect with synchronized audio and visual.
 */
class PowerToggleEffect final : public Core::InertialEffect {
public:
    PowerToggleEffect(Profiles::PowerStateMachine& power, InertialSwingEffect& swing,
                      InertialLightEffect& light, System::AudioController& audio,
                      Espressif::Wrappers::SmartLed::Engine& ledEngine,
                      const Profiles::Inertial::InertialDefinition& definition,
                      const Profiles::SoundFont& font, uint8_t buttonId);

    bool test(const Core::SaberDataPacket& packet) override;
    void run() override;

private:
    void beginIgnition();
    void tickIgnition();
    void beginRetraction();
    void tickRetraction();

    Profiles::PowerStateMachine& m_power;
    InertialSwingEffect& m_swing;
    InertialLightEffect& m_light;
    System::AudioController& m_audio;
    Espressif::Wrappers::SmartLed::Engine& m_ledEngine;
    const Profiles::Inertial::InertialDefinition& m_def;
    const Profiles::SoundFont& m_font;
    uint8_t m_buttonId;

    bool m_pendingTransition = false;
    bool m_enginesStarted = false;
    uint32_t m_sequenceStartMs = 0;
};

} // namespace InertialSaber::Effects
