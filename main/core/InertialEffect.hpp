#pragma once

#include "SaberDataPacket.hpp"

#include <cstdint>

namespace InertialSaber::Core {

/**
 * @brief Abstract contract for any action registered on the SaberAction Bus.
 *
 * Concrete effects implement Test() to evaluate trigger conditions against
 * the SaberDataPacket, and Run() to execute the rendering response
 * (audio, visual, haptic). Priority levels follow the system hierarchy:
 *   0 = Background, 1 = Standard, 2 = Override, 3 = System.
 */
class InertialEffect {
public:
    uint8_t Priority = 0;

    virtual ~InertialEffect() = default;

    /**
     * @brief Evaluate whether the effect's trigger condition is met.
     * @param packet Immutable snapshot of the current cycle's sensor and input state.
     * @return true if the effect should fire this cycle.
     */
    virtual bool Test(const SaberDataPacket& packet) = 0;

    /**
     * @brief Execute the effect's rendering response (audio, visual, haptic).
     */
    virtual void Run() = 0;
};

} // namespace InertialSaber::Core
