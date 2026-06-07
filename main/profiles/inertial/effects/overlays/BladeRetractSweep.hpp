#pragma once

#include "Canvas.hpp"
#include "IEffect.hpp"
#include "SmartLedTypes.hpp"

#include <cstdint>

namespace InertialSaber::Effects {

/**
 * @brief Top-to-bottom blade retraction wipe overlay for the SmartLed Engine.
 */
class BladeRetractSweep final : public Espressif::Wrappers::SmartLed::IEffect {
public:
    BladeRetractSweep(uint16_t numLeds, uint16_t hue, uint32_t durationMs);

    void update(uint32_t delta_ms) override;
    void render(Espressif::Wrappers::SmartLed::Canvas& canvas) override;
    [[nodiscard]] bool isFinished() const override;

private:
    uint16_t m_numLeds;
    Espressif::Wrappers::SmartLed::Color m_color;
    uint32_t m_durationMs;
    uint32_t m_elapsed = 0;
};

} // namespace InertialSaber::Effects
