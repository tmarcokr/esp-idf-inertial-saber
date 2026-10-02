#pragma once

#include "core/SaberActionBus.hpp"
#include "system/PsramAudioCache.hpp"
#include "system/audio/AudioController.hpp"
#include "system/status/StatusIndicator.hpp"
#include "Engine.hpp"

namespace InertialSaber::Profiles {

/**
 * @brief Typed parameter object with the long-lived services a profile wires into its effects.
 */
struct SaberServices {
    Core::SaberActionBus& bus;
    System::AudioController& audioControl;
    Espressif::Wrappers::SmartLed::Engine& blade;
    System::PsramAudioCache& audioCache;
    System::Status::StatusIndicator& status;
};

} // namespace InertialSaber::Profiles
