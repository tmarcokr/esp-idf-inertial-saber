#pragma once

#include <cstdint>

namespace InertialSaber::Effects {

/** @brief Full-scale playback volume (14-bit scale). */
inline constexpr uint16_t kFullVolume = 16384;

/** @brief Retraction one-shot volume (70 % of full scale) balancing loudness and distortion. */
inline constexpr uint16_t kRetractionVolume = 11468;

} // namespace InertialSaber::Effects
