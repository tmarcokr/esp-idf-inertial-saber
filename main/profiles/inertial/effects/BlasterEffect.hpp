#pragma once

#include "core/InertialEffect.hpp"
#include <cstdint>

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
namespace InertialSaber::System {
class AudioController;
}
namespace Espressif::Wrappers::SmartLed {
class Engine;
}

namespace InertialSaber::Effects {

/**
 * @brief Handles blaster block trigger and rendering (sound and visual overlay).
 */
class BlasterEffect final : public Core::InertialEffect {
public:
    BlasterEffect(const Profiles::PowerStateMachine& power, System::AudioController& audio,
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
};

} // namespace InertialSaber::Effects
