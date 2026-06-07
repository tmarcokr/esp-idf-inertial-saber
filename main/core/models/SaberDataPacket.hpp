#pragma once

#include "system/config/HardwareConfig.hpp"

#include <array>
#include <cstdint>

namespace InertialSaber::Core {

/**
 * @brief Full state-machine snapshot for a single input peripheral.
 *
 * Provides enough context for InertialEffect evaluation of complex
 * interaction patterns (multi-click, long press, transitions) without
 * direct hardware access.
 */
struct InputDescriptor {
    enum class State : uint8_t { IDLE, PRESSED, HELD, RELEASED };

    State current = State::IDLE;
    State previous = State::IDLE;
    uint32_t holdDuration_ms = 0;
    uint32_t lastTransition_ms = 0;

    /**
     * @brief Semantic gesture resolved by the input adapter.
     */
    enum class Gesture : uint8_t {
        NONE      = 0,
        CLICK     = 1,
        HOLD_TICK = 2
    };

    Gesture gesture = Gesture::NONE;
    uint8_t pressCount = 0;
    uint8_t holdLevel = 0;
};

/**
 * @brief Unified data packet distributed by the SaberAction Bus each cycle.
 *
 * Contains an immutable snapshot of all motion and input descriptors.
 * Every InertialEffect evaluated in the same cycle receives an identical
 * copy, guaranteeing deterministic evaluation order.
 */
struct SaberDataPacket {
    /// Absolute linear acceleration magnitude in Gs (gravity subtracted).
    float KineticEnergy = 0.0f;
    
    /// Angular velocity across XYZ axes in degrees per second.
    float AxisRotation[3] = {0.0f, 0.0f, 0.0f};
    
    /// Vertical alignment (-1.0 to 1.0) where 1.0 is pointing straight UP and -1.0 is straight DOWN.
    float OrientationVector = 0.0f;

    /// Virtual inertia accumulator (0.0f to 1.0f).
    float InertialOverload = 0.0f;
    
    /// True for exactly one cycle when InertialOverload reaches 1.0f.
    bool InertialBurst = false;

    /// Array of input peripheral states (buttons, switches).
    std::array<InputDescriptor, System::Config::HardwareConfig::kMaxInputs> inputs{};

    /// FreeRTOS system time in milliseconds at the start of this bus cycle.
    uint32_t timestamp_ms = 0;
};

} // namespace InertialSaber::Core
