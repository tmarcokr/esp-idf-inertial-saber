#include "profiles/inertial/InertialDefinition.hpp"
#pragma once

#include "core/InertialEffect.hpp"
#include <cstdint>

namespace InertialSaber::Profiles {
class SoundFont;
}
namespace InertialSaber::Core {

struct SaberDataPacket;
}
namespace InertialSaber::Effects {
class PowerToggleEffect;
}
namespace Espressif::Wrappers::Audio {
class AudioEngine;
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
    BlasterEffect(
        PowerToggleEffect&                      power,
        Espressif::Wrappers::Audio::AudioEngine& audio,
        Espressif::Wrappers::SmartLed::Engine&   ledEngine,
        const InertialSaber::Profiles::Inertial::InertialDefinition&          definition,
        const Profiles::SoundFont&               font,
        uint8_t                                  buttonId);

    bool test(const Core::SaberDataPacket& packet) override;
    void run() override;

private:
    PowerToggleEffect&                       m_power;
    Espressif::Wrappers::Audio::AudioEngine& m_audio;
    Espressif::Wrappers::SmartLed::Engine&   m_ledEngine;
    const InertialSaber::Profiles::Inertial::InertialDefinition&          m_def;
    const Profiles::SoundFont&               m_font;
    uint8_t                                  m_buttonId;
};

} // namespace InertialSaber::Effects
