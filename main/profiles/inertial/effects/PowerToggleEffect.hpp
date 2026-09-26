#include "profiles/inertial/InertialDefinition.hpp"
#pragma once

#include "core/InertialEffect.hpp"
#include <cstdint>

namespace InertialSaber::Core {

struct SaberDataPacket;
}
namespace InertialSaber::Profiles {
class ConfigurableProfile;
class SoundFont;
}
namespace InertialSaber::Effects {
class InertialSwingEffect;
class InertialLightEffect;
}
namespace Espressif::Wrappers::Audio {
class AudioEngine;
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
    PowerToggleEffect(
        Profiles::ConfigurableProfile&           profile,
        InertialSwingEffect&                     swing,
        InertialLightEffect&                     light,
        Espressif::Wrappers::Audio::AudioEngine& audio,
        Espressif::Wrappers::SmartLed::Engine&   ledEngine,
        const InertialSaber::Profiles::Inertial::InertialDefinition&          definition,
        const Profiles::SoundFont&               font,
        uint8_t                                  buttonId);

    bool test(const Core::SaberDataPacket& packet) override;
    void run() override;
    [[nodiscard]] bool isIgnited() const;
    [[nodiscard]] bool isRetracted() const;

private:
    void beginIgnition();
    void tickIgnition();
    void beginRetraction();
    void tickRetraction();

    Profiles::ConfigurableProfile&           m_profile;
    InertialSwingEffect&                     m_swing;
    InertialLightEffect&                     m_light;
    Espressif::Wrappers::Audio::AudioEngine& m_audio;
    Espressif::Wrappers::SmartLed::Engine&   m_ledEngine;
    const InertialSaber::Profiles::Inertial::InertialDefinition&          m_def;
    const Profiles::SoundFont&               m_font;
    uint8_t                                  m_buttonId;

    bool     m_pendingTransition = false;
    bool     m_enginesStarted = false;
    uint32_t m_sequenceStartMs = 0;
};

} // namespace InertialSaber::Effects
