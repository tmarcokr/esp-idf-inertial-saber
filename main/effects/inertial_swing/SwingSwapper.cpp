#include "SwingSwapper.hpp"

namespace InertialSaber::Effects::InertialSwing {

bool SwingSwapper::evaluateSwap(float masterVolume, uint32_t timestampMs,
                                float swapMinVolume, uint32_t swapCooldownMs) {
    if (masterVolume > swapMinVolume) {
        m_needsSwap = true;
    }

    bool isMoving = masterVolume > 0.0f;

    if (isMoving) {
        m_wasMoving = true;
        m_lastMovementTimeMs = timestampMs;
    } else {    
        if (m_wasMoving) {
            m_wasMoving = false;
        } else if (m_needsSwap) {
            uint32_t idleTime = timestampMs - m_lastMovementTimeMs;
            if (idleTime >= swapCooldownMs) {
                m_needsSwap = false;
                return true;
            }
        }
    }

    return false;
}

void SwingSwapper::reset() {
    m_needsSwap = false;
    m_wasMoving = false;
    m_lastMovementTimeMs = 0;
}

} // namespace InertialSaber::Effects::InertialSwing
