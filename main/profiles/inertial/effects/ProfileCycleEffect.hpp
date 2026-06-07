#pragma once

#include "core/InertialEffect.hpp"
#include <cstdint>

namespace InertialSaber::Core {
struct SaberDataPacket;
class SaberActionBus;
}
namespace InertialSaber::Profiles {
class ConfigurableProfile;
class ProfileManager;
}
namespace Espressif::Wrappers::Audio {
class AudioEngine;
}
namespace Espressif::Wrappers::SmartLed {
class Engine;
}

namespace InertialSaber::Effects {

/**
 * @brief Cycles to the next profile on CLICK pressCount=3 while the saber is RETRACTED.
 */
class ProfileCycleEffect final : public Core::InertialEffect {
public:
    /**
     * @brief Construct a new ProfileCycleEffect.
     */
    ProfileCycleEffect(
        Profiles::ConfigurableProfile&           profile,
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
    Profiles::ConfigurableProfile&           m_profile;
    Profiles::ProfileManager&                m_profileManager;
    Core::SaberActionBus&                    m_bus;
    Espressif::Wrappers::Audio::AudioEngine& m_audio;
    Espressif::Wrappers::SmartLed::Engine&   m_led;
    uint8_t                                  m_buttonId;
};

} // namespace InertialSaber::Effects
