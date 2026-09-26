#pragma once

#include "core/InertialEffect.hpp"
#include <cstdint>

namespace InertialSaber::Core {
struct SaberDataPacket;
}
namespace InertialSaber::Profiles {
class PowerStateMachine;
class ProfileManager;
}

namespace InertialSaber::Effects {

/**
 * @brief Cycles to the next profile on a Click with pressCount=3 while the saber is retracted or its preload has faulted.
 */
class ProfileCycleEffect final : public Core::InertialEffect {
public:
    /**
     * @brief Construct a new ProfileCycleEffect.
     */
    ProfileCycleEffect(const Profiles::PowerStateMachine& power,
                       Profiles::ProfileManager& profileManager,
                       uint8_t buttonId);

    /**
     * @brief Test if the profile cycle gesture is triggered.
     */
    bool test(const Core::SaberDataPacket& packet) override;

    /**
     * @brief Execute the profile cycle.
     */
    void run() override;

private:
    const Profiles::PowerStateMachine& m_power;
    Profiles::ProfileManager&      m_profileManager;
    uint8_t                        m_buttonId;
};

} // namespace InertialSaber::Effects
