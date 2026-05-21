#pragma once

#include "interfaces/InertialEffect.hpp"
#include <cstdint>
#include <string>

namespace InertialSaber::Core {
struct InertialDefinition;
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
        const Core::InertialDefinition&          definition,
        uint8_t                                  buttonId);

    bool Test(const Core::SaberDataPacket& packet) override;
    void Run() override;

private:
    [[nodiscard]] std::string buildPath(const char* subAndPrefix, uint8_t index) const;

    PowerToggleEffect&                       m_power;
    Espressif::Wrappers::Audio::AudioEngine& m_audio;
    Espressif::Wrappers::SmartLed::Engine&   m_ledEngine;
    const Core::InertialDefinition&          m_def;
    uint8_t                                  m_buttonId;
};

} // namespace InertialSaber::Effects
