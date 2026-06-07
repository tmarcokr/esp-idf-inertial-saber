#pragma once

#include "interfaces/InertialEffect.hpp"
#include "interfaces/InertialProfile.hpp"
#include "profiles/ProfileManager.hpp"
#include <cstdint>

namespace InertialSaber::Core {
struct SaberDataPacket;
}
namespace Espressif::Wrappers::Audio {
class AudioEngine;
}
namespace Espressif::Wrappers::SmartLed {
class Engine;
}

namespace InertialSaber::Effects {

/**
 * @brief Cycles to the next profile on CLICK pressCount=3 while the saber is IGNITED.
 */
class ProfileCycleEffect final : public Core::InertialEffect {
public:
    /**
     * @brief Construct a new ProfileCycleEffect.
     */
    ProfileCycleEffect(
        Core::InertialProfile&                   profile,
        Profiles::ProfileManager&                profileManager,
        Core::SaberActionBus&                    bus,
        Espressif::Wrappers::Audio::AudioEngine& audio,
        Espressif::Wrappers::SmartLed::Engine&   led,
        uint8_t                                  buttonId);

    /**
     * @brief Test if the profile cycle gesture is triggered.
     */
    bool Test(const Core::SaberDataPacket& packet) override;

    /**
     * @brief Execute the profile cycle.
     */
    void Run() override;

private:
    Core::InertialProfile&                   m_profile;
    Profiles::ProfileManager&                m_profileManager;
    Core::SaberActionBus&                    m_bus;
    Espressif::Wrappers::Audio::AudioEngine& m_audio;
    Espressif::Wrappers::SmartLed::Engine&   m_led;
    uint8_t                                  m_buttonId;
};

} // namespace InertialSaber::Effects
