#include "profiles/inertial/InertialDefinition.hpp"
#pragma once

#include "core/InertialEffect.hpp"
#include <array>
#include <cstdint>
#include <string>

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
 * @brief Evaluates physical impact triggers (clash) and plays audio + LED flash overlays.
 */
class KineticImpactEffect final : public Core::InertialEffect {
public:
    KineticImpactEffect(
        PowerToggleEffect&                      power,
        Espressif::Wrappers::Audio::AudioEngine& audio,
        Espressif::Wrappers::SmartLed::Engine&  ledEngine,
        const InertialSaber::Profiles::Inertial::InertialDefinition&         definition);

    bool Test(const Core::SaberDataPacket& packet) override;
    void Run() override;

private:
    [[nodiscard]] std::string buildPath(const char* subAndPrefix, uint8_t index) const;

    void clearKineticEnergyWindow();
    bool detectClash(const Core::SaberDataPacket& packet);

    PowerToggleEffect&                       m_power;
    Espressif::Wrappers::Audio::AudioEngine& m_audio;
    Espressif::Wrappers::SmartLed::Engine&   m_ledEngine;
    const InertialSaber::Profiles::Inertial::InertialDefinition&          m_def;

    std::array<float, 4> m_kineticEnergyWindow{};
    size_t               m_windowIdx = 0;
    uint32_t             m_lastClashTimeMs = 0;
};

} // namespace InertialSaber::Effects
