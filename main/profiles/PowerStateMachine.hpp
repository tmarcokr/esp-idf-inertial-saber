#pragma once

#include <cstdint>

namespace InertialSaber::Profiles {

/**
 * @brief Power lifecycle of the active profile with guarded transitions.
 *
 * Accessed from the bus task only (and from the main task before the bus starts); not thread-safe.
 */
class PowerStateMachine final {
public:
    enum class State : uint8_t { Locked, Retracted, Igniting, Ignited, Retracting, Faulted };
    enum class Event : uint8_t { Lock, PreloadDone, PreloadFailed, IgniteRequested, IgnitionElapsed, RetractRequested, RetractionElapsed };

    /** @brief Applies @p event; returns false (state unchanged, logged as a warning) if it is not allowed from the current state. */
    bool handle(Event event);

    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] bool isIgnited() const { return m_state == State::Ignited; }
    [[nodiscard]] bool isRetracted() const { return m_state == State::Retracted; }
    [[nodiscard]] bool isFaulted() const { return m_state == State::Faulted; }

private:
    State m_state = State::Locked;
};

} // namespace InertialSaber::Profiles
