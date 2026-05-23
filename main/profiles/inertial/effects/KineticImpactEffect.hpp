#pragma once

#include "interfaces/InertialEffect.hpp"
#include <array>
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
 * @brief Evaluates physical impact triggers (clash) and plays audio + LED flash overlays.
 */
class KineticImpactEffect final : public Core::InertialEffect {
public:
    KineticImpactEffect(
        PowerToggleEffect&                      power,
        Espressif::Wrappers::Audio::AudioEngine& audio,
        Espressif::Wrappers::SmartLed::Engine&  ledEngine,
        const Core::InertialDefinition&         definition);

    bool Test(const Core::SaberDataPacket& packet) override;
    void Run() override;

private:
    [[nodiscard]] std::string buildPath(const char* subAndPrefix, uint8_t index) const;

    void clearKineticEnergyWindow();
    bool detectClash(const Core::SaberDataPacket& packet);

    PowerToggleEffect&                       m_power;
    Espressif::Wrappers::Audio::AudioEngine& m_audio;
    Espressif::Wrappers::SmartLed::Engine&   m_ledEngine;
    const Core::InertialDefinition&          m_def;

    std::array<float, 4> m_kineticEnergyWindow{};
    size_t               m_windowIdx = 0;
    uint32_t             m_lastClashTimeMs = 0;
};

} // namespace InertialSaber::Effects
