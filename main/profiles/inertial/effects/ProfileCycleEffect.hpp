// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/InertialEffect.hpp"
#include <cstdint>

namespace InertialSaber::Core {
struct SaberDataPacket;
}
namespace InertialSaber::Profiles {
class PowerStateMachine;
class ProfileManager;
} // namespace InertialSaber::Profiles

namespace InertialSaber::Effects {

/**
 * @brief Requests the next profile on a Click with pressCount=3 while the saber is retracted or its preload has faulted.
 *
 * Ignored while a previous switch is pending. Moves its set to Switching and posts the request
 * to the ProfileManager; the switch itself runs off the bus.
 */
class ProfileCycleEffect final : public Core::InertialEffect {
public:
    /**
     * @brief Construct a new ProfileCycleEffect.
     */
    ProfileCycleEffect(Profiles::PowerStateMachine& power, Profiles::ProfileManager& profileManager,
                       uint8_t buttonId);

    /**
     * @brief Test if the profile cycle gesture is triggered.
     */
    bool test(const Core::SaberDataPacket& packet) override;

    /**
     * @brief Request the profile cycle.
     */
    void run() override;

private:
    Profiles::PowerStateMachine& m_power;
    Profiles::ProfileManager& m_profileManager;
    uint8_t m_buttonId;
};

} // namespace InertialSaber::Effects
