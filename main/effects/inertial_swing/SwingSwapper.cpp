#include "SwingSwapper.hpp"
#include "PlatformConfig.hpp"

namespace InertialSaber::Effects::InertialSwing {

bool SwingSwapper::evaluateSwap(float masterVolume, uint32_t timestampMs) {
    if (masterVolume > Core::Platform::kSwingSwapMinVolume) {
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
            if (idleTime >= Core::Platform::kSwingSwapCooldownMs) {
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
