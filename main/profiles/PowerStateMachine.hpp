#pragma once

#include <cstdint>

namespace InertialSaber::Profiles {

/**
 * @brief Power lifecycle of the active profile with guarded transitions.
 *
 * Accessed from the bus task only (and from the main task before the bus starts); not thread-safe.
 */
class PowerStateMachine {
public:
    enum class State : uint8_t { Locked, Retracted, Igniting, Ignited, Retracting };
    enum class Event : uint8_t { Lock, PreloadDone, IgniteRequested, IgnitionElapsed, RetractRequested, RetractionElapsed };

    /** @brief Applies @p event; returns false (state unchanged, logged at DEBUG) if it is not allowed from the current state. */
    bool handle(Event event);

    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] bool isIgnited() const { return m_state == State::Ignited; }
    [[nodiscard]] bool isRetracted() const { return m_state == State::Retracted; }

private:
    State m_state = State::Locked;
};

} // namespace InertialSaber::Profiles
