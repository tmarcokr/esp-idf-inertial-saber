#include "profiles/PowerStateMachine.hpp"

#include "esp_log.h"

#include <optional>

namespace InertialSaber::Profiles {

namespace {

constexpr const char* TAG = "PowerSM";

using State = PowerStateMachine::State;
using Event = PowerStateMachine::Event;

constexpr std::optional<State> nextState(State from, Event event) {
    switch (event) {
    case Event::Lock:
        if (from == State::Locked || from == State::Retracted) return State::Locked;
        break;
    case Event::PreloadDone:
        if (from == State::Locked) return State::Retracted;
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
    }
    return std::nullopt;
}

} // namespace

bool PowerStateMachine::handle(Event event) {
    const std::optional<State> next = nextState(m_state, event);
    if (!next) {
        ESP_LOGW(TAG, "Rejected event %u in state %u", static_cast<unsigned>(event),
                 static_cast<unsigned>(m_state));
        return false;
    }
    m_state = *next;
    return true;
}

} // namespace InertialSaber::Profiles
