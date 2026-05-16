#pragma once

#include "InertialProfile.hpp"

namespace InertialSaber::Profiles {

/**
 * @brief The "inertial" factory profile.
 *
 * Encodes the default physics, audio, and visual parameters for the base
 * InertialSaber experience. Blue blade, standard Inertial Overload sensitivity,
 * and the ProffieOS-compatible font directory layout under profiles/inertial/.
 */
class InertialDefaultProfile final : public Core::InertialProfile {
public:
    [[nodiscard]] const Core::InertialDefinition& getDefinition() const override;

    void load(Core::SaberActionBus& bus,
              Espressif::Wrappers::Audio::AudioEngine& audio,
              Espressif::Wrappers::SmartLed::Engine& led) override;

    void unload(Core::SaberActionBus& bus) override;
};

} // namespace InertialSaber::Profiles
