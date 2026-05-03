#pragma once

#include "PlatformConfig.hpp"

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
    uint8_t pressCount = 0;
    uint32_t lastTransition_ms = 0;
};

/**
 * @brief Unified data packet distributed by the SaberAction Bus each cycle.
 *
 * Contains an immutable snapshot of all motion and input descriptors.
 * Every InertialEffect evaluated in the same cycle receives an identical
 * copy, guaranteeing deterministic evaluation order.
 */
struct SaberDataPacket {
    float KineticEnergy = 0.0f;
    float AxisRotation[3] = {0.0f, 0.0f, 0.0f};
    float OrientationVector = 0.0f;

    float TanqueOverload = 0.0f;
    bool OverloadBurst = false;

    std::array<InputDescriptor, Platform::kMaxInputs> inputs{};

    uint32_t timestamp_ms = 0;
};

} // namespace InertialSaber::Core
