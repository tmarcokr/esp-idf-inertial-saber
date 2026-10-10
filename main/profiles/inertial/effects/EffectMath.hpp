// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <numbers>

namespace InertialSaber::Effects {

/** @brief Half of pi; maps the normalised blade orientation to its angle in radians. */
inline constexpr float kHalfPi = std::numbers::pi_v<float> / 2.0f;

} // namespace InertialSaber::Effects
