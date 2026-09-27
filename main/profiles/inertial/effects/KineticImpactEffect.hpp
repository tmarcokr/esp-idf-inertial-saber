#pragma once

#include "core/InertialEffect.hpp"
#include <array>
#include <cstddef>
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
 * @brief Evaluates physical impact triggers (clash) and plays audio + LED flash overlays.
 */
class KineticImpactEffect final : public Core::InertialEffect {
public:
    KineticImpactEffect(const Profiles::PowerStateMachine& power, System::AudioController& audio,
                        Espressif::Wrappers::SmartLed::Engine& ledEngine,
                        const Profiles::Inertial::InertialDefinition& definition,
                        const Profiles::SoundFont& font);

    bool test(const Core::SaberDataPacket& packet) override;
    void run() override;

private:
    static constexpr size_t kKineticEnergyWindowSize = 4;
    static constexpr uint32_t kClashDebounceMs = 500;

    void clearKineticEnergyWindow();
    bool detectClash(const Core::SaberDataPacket& packet);

    const Profiles::PowerStateMachine& m_power;
    System::AudioController& m_audio;
    Espressif::Wrappers::SmartLed::Engine& m_ledEngine;
    const Profiles::Inertial::InertialDefinition& m_def;
    const Profiles::SoundFont& m_font;

    std::array<float, kKineticEnergyWindowSize> m_kineticEnergyWindow{};
    size_t m_windowIdx = 0;
    uint32_t m_lastClashTimeMs = 0;
};

} // namespace InertialSaber::Effects
