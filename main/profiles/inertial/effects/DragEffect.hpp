#include "profiles/inertial/InertialDefinition.hpp"
#pragma once

#include "core/InertialEffect.hpp"
#include "AudioEngine.hpp"
#include <cstdint>

namespace InertialSaber::Profiles {
class SoundFont;
}
namespace InertialSaber::Core {

struct SaberDataPacket;
}
namespace InertialSaber::Effects {
class PowerToggleEffect;
class BladeDragEffect;
}
namespace Espressif::Wrappers::Audio {
class AudioEngine;
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
    /**
     * @brief Construct a new Drag Effect.
     * @param power Power toggle effect reference.
     * @param audio Audio engine reference.
     * @param ledEngine SmartLed engine reference.
     * @param definition Active inertial definition.
     * @param font Sound font of the active profile.
     * @param buttonId Trigger button identifier.
     */
    DragEffect(
        PowerToggleEffect& power,
        Espressif::Wrappers::Audio::AudioEngine& audio,
        Espressif::Wrappers::SmartLed::Engine& ledEngine,
        const InertialSaber::Profiles::Inertial::InertialDefinition& definition,
        const Profiles::SoundFont& font,
        uint8_t buttonId);

    bool test(const Core::SaberDataPacket& packet) override;
    void run() override;

private:
    PowerToggleEffect& m_power;
    Espressif::Wrappers::Audio::AudioEngine& m_audio;
    Espressif::Wrappers::SmartLed::Engine& m_ledEngine;
    const InertialSaber::Profiles::Inertial::InertialDefinition& m_def;
    const Profiles::SoundFont& m_font;
    uint8_t m_buttonId;

    Espressif::Wrappers::Audio::ChannelId m_audioChannel = Espressif::Wrappers::Audio::INVALID_CHANNEL;
    BladeDragEffect* m_ledEffect = nullptr;
    bool m_active = false;
    bool m_triggerMet = false;
};

} // namespace InertialSaber::Effects
