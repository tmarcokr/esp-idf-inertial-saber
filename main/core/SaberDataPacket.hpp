#pragma once

#include "system/hardware/HardwareConfig.hpp"

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
    enum class State : uint8_t { Idle, Pressed, Held, Released };

    State current = State::Idle;
    State previous = State::Idle;
    uint32_t holdDurationMs = 0;
    uint32_t lastTransitionMs = 0;

    /**
     * @brief Semantic gesture resolved by the input adapter.
     */
    enum class Gesture : uint8_t {
        None     = 0,
        Click    = 1,
        HoldTick = 2
    };

    Gesture gesture = Gesture::None;
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
    float kineticEnergy = 0.0f;
    
    /// Angular velocity across XYZ axes in degrees per second.
    std::array<float, 3> axisRotation{};
    
    /// Vertical alignment (-1.0 to 1.0) where 1.0 is pointing straight UP and -1.0 is straight DOWN.
    float orientation = 0.0f;

    /// Virtual inertia accumulator (0.0f to 1.0f).
    float inertialOverload = 0.0f;
    
    /// True for exactly one cycle when inertialOverload reaches 1.0f.
    bool inertialBurst = false;

    /// Array of input peripheral states (buttons, switches).
    std::array<InputDescriptor, System::Hardware::HardwareConfig::kMaxInputs> inputs{};

    /// FreeRTOS system time in milliseconds at the start of this bus cycle.
    uint32_t timestampMs = 0;
};

} // namespace InertialSaber::Core
