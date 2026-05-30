#pragma once

#include "interfaces/InertialEffect.hpp"
#include <cstdint>
#include <string>

namespace InertialSaber::Core {
struct InertialDefinition;
struct SaberDataPacket;
class InertialProfile;
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
        Core::InertialProfile&                  profile,
        InertialSwingEffect&                    swing,
        InertialLightEffect&                    light,
        Espressif::Wrappers::Audio::AudioEngine& audio,
        Espressif::Wrappers::SmartLed::Engine&  ledEngine,
        const Core::InertialDefinition&         definition,
        uint8_t                                 buttonId);

    bool Test(const Core::SaberDataPacket& packet) override;
    void Run() override;
    [[nodiscard]] bool isIgnited() const;
    [[nodiscard]] bool isRetracted() const;

private:
    void beginIgnition();
    void tickIgnition();
    void beginRetraction();
    void tickRetraction();

    [[nodiscard]] std::string buildPath(const char* subAndPrefix, uint8_t index) const;

    Core::InertialProfile&                  m_profile;
    InertialSwingEffect&                    m_swing;
    InertialLightEffect&                    m_light;
    Espressif::Wrappers::Audio::AudioEngine& m_audio;
    Espressif::Wrappers::SmartLed::Engine&  m_ledEngine;
    const Core::InertialDefinition&         m_def;
    uint8_t                                 m_buttonId;

    bool     m_pendingTransition = false;
    bool     m_enginesStarted = false;
    uint32_t m_sequenceStartMs = 0;
    uint32_t m_lastClickTimeMs = 0;
    bool     m_hasLastClick = false;
};

} // namespace InertialSaber::Effects
