#pragma once

#include <cstdint>

namespace InertialSaber::Effects::InertialSwing {

/**
 * @brief State machine that determines when it is safe to swap the swing audio pair.
 */
class SwingSwapper {
public:
    SwingSwapper() = default;

    /**
     * @brief Evaluates the state machine to determine if a swap is needed.
     * @param masterVolume Current master volume.
     * @param timestampMs Current time in milliseconds.
     * @return True if an audio swap should be executed.
     */
    /**
     * @param swapMinVolume Minimum master volume required to arm a swap.
     * @param swapCooldownMs Minimum idle time in ms before the swap fires.
     */
    [[nodiscard]] bool evaluateSwap(float masterVolume, uint32_t timestampMs,
                                    float swapMinVolume, uint32_t swapCooldownMs);

    /**
     * @brief Resets the internal state machine.
     */
    void reset();

private:
    bool m_needsSwap = false;
    bool m_wasMoving = false;
    uint32_t m_lastMovementTimeMs = 0;
};

} // namespace InertialSaber::Effects::InertialSwing
