// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace InertialSaber::Core {

struct PhysicsConfig {
    float overloadThresholdG;
    float overloadChargeRate;
    float overloadDrainRate;
    float burstCooldownMs;
    float kineticEnergyDeadbandG;
    float rotationDeadbandDps;
};

} // namespace InertialSaber::Core
