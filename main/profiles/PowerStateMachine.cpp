#include "profiles/PowerStateMachine.hpp"
#include "diagnostics/Metrics.hpp"

#include "esp_log.h"

#include <optional>

namespace InertialSaber::Profiles {

namespace {

constexpr const char* TAG = "PowerSM";

using State = PowerStateMachine::State;
using Event = PowerStateMachine::Event;

constexpr std::optional<State> nextState(State from, Event event) {
    switch (event) {
    case Event::PreloadDone:
        if (from == State::Locked) return State::Retracted;
        break;
    case Event::PreloadFailed:
        if (from == State::Locked) return State::Faulted;
        break;
    case Event::IgniteRequested:
        if (from == State::Retracted) return State::Igniting;
        break;
    case Event::IgnitionElapsed:
        if (from == State::Igniting) return State::Ignited;
        break;
    case Event::RetractRequested:
        if (from == State::Ignited) return State::Retracting;
        break;
    case Event::RetractionElapsed:
        if (from == State::Retracting) return State::Retracted;
        break;
    case Event::SwitchRequested:
        if (from == State::Retracted || from == State::Faulted) return State::Switching;
        break;
    }
    return std::nullopt;
}

} // namespace

bool PowerStateMachine::handle(Event event) {
    const State current = state();
    const std::optional<State> next = nextState(current, event);
    if (!next) {
        ESP_LOGW(TAG, "Rejected event %u in state %u", static_cast<unsigned>(event),
                 static_cast<unsigned>(current));
        return false;
    }
    m_state.store(*next, std::memory_order_relaxed);
    if (event == Event::IgniteRequested) SABER_METRIC_SESSION_BEGIN();
    if (event == Event::RetractionElapsed) SABER_METRIC_SESSION_END();
    return true;
}

} // namespace InertialSaber::Profiles
