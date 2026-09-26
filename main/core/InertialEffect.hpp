#pragma once

#include "SaberDataPacket.hpp"

#include <cstdint>

namespace InertialSaber::Core {

/**
 * @brief Abstract contract for any action registered on the SaberAction Bus.
 *
 * Concrete effects implement test() to evaluate trigger conditions against
 * the SaberDataPacket, and run() to execute the rendering response
 * (audio, visual, haptic). Priority levels follow the system hierarchy:
 *   0 = Background, 1 = Standard, 2 = Override, 3 = System.
 */
class InertialEffect {
public:
    /**
     * @brief Construct an effect with a fixed evaluation priority.
     * @param priority Lower values are evaluated first.
     */
    explicit InertialEffect(uint8_t priority) : m_priority(priority) {}

    virtual ~InertialEffect() = default;

    InertialEffect(const InertialEffect&) = delete;
    InertialEffect& operator=(const InertialEffect&) = delete;

    /**
     * @brief Evaluation priority of this effect.
     */
    [[nodiscard]] uint8_t priority() const { return m_priority; }

    /**
     * @brief Evaluate whether the effect's trigger condition is met.
     * @param packet Immutable snapshot of the current cycle's sensor and input state.
     * @return true if the effect should fire this cycle.
     */
    virtual bool test(const SaberDataPacket& packet) = 0;

    /**
     * @brief Execute the effect's rendering response (audio, visual, haptic).
     */
    virtual void run() = 0;

private:
    const uint8_t m_priority;
};

} // namespace InertialSaber::Core
