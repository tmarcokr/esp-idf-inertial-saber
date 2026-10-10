// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <cstdint>

namespace InertialSaber::Profiles {

/**
 * @brief Power lifecycle of one effect set with guarded transitions.
 *
 * handle() is called from the bus task only (or from the builder task before the set is handed
 * to the bus); the state can be read from any task. Switching is terminal: the set is retired
 * from it.
 */
class PowerStateMachine final {
public:
    enum class State : uint8_t {
        Locked,
        Retracted,
        Igniting,
        Ignited,
        Retracting,
        Faulted,
        Switching
    };
    enum class Event : uint8_t {
        PreloadDone,
        PreloadFailed,
        IgniteRequested,
        IgnitionElapsed,
        RetractRequested,
        RetractionElapsed,
        SwitchRequested
    };

    /** @brief Applies @p event; returns false (state unchanged, logged as a warning) if it is not allowed from the current state. */
    bool handle(Event event);

    /** @brief Current state; any task (relaxed read). */
    [[nodiscard]] State state() const { return m_state.load(std::memory_order_relaxed); }
    [[nodiscard]] bool isIgnited() const { return state() == State::Ignited; }
    [[nodiscard]] bool isRetracted() const { return state() == State::Retracted; }
    [[nodiscard]] bool isFaulted() const { return state() == State::Faulted; }

private:
    std::atomic<State> m_state{State::Locked};
};

} // namespace InertialSaber::Profiles
