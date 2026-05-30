#pragma once

#include "interfaces/InertialEffect.hpp"
#include "AudioEngine.hpp"
#include <cstdint>
#include <string>

namespace InertialSaber::Core {
struct InertialDefinition;
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
 * @brief Evaluates drag (friction burn) trigger conditions and manages looping sound and LED overlay.
 */
class FrictionBurnEffect final : public Core::InertialEffect {
public:
    /**
     * @brief Construct a new Friction Burn Effect.
     * @param power Power toggle effect reference.
     * @param audio Audio engine reference.
     * @param ledEngine SmartLed engine reference.
     * @param definition Active inertial definition.
     */
    FrictionBurnEffect(
        PowerToggleEffect& power,
        Espressif::Wrappers::Audio::AudioEngine& audio,
        Espressif::Wrappers::SmartLed::Engine& ledEngine,
        const Core::InertialDefinition& definition,
        uint8_t buttonId);

    bool Test(const Core::SaberDataPacket& packet) override;
    void Run() override;

private:
    [[nodiscard]] std::string buildPath(const char* subAndPrefix, uint8_t index) const;

    PowerToggleEffect& m_power;
    Espressif::Wrappers::Audio::AudioEngine& m_audio;
    Espressif::Wrappers::SmartLed::Engine& m_ledEngine;
    const Core::InertialDefinition& m_def;
    uint8_t m_buttonId;

    Espressif::Wrappers::Audio::ChannelId m_audioChannel = Espressif::Wrappers::Audio::INVALID_CHANNEL;
    BladeDragEffect* m_ledEffect = nullptr;
    bool m_active = false;
    bool m_triggerMet = false;
};

} // namespace InertialSaber::Effects
