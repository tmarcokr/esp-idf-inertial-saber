#pragma once

#include "Canvas.hpp"
#include "IEffect.hpp"
#include "SmartLedTypes.hpp"

#include <cstdint>

namespace InertialSaber::Effects {

/**
 * @brief Visual overlay effect for a blade clash. Fades a complementary color flash over the entire blade.
 */
class BladeClashFlash final : public Espressif::Wrappers::SmartLed::IEffect {
public:
    BladeClashFlash(uint16_t numLeds, uint16_t baseHue, uint32_t durationMs);

    void update(uint32_t delta_ms) override;
    void render(Espressif::Wrappers::SmartLed::Canvas& canvas) override;
    [[nodiscard]] bool isFinished() const override;

private:
    uint16_t m_numLeds;
    uint16_t m_clashHue;
    uint32_t m_durationMs;
    uint32_t m_elapsed = 0;
};

} // namespace InertialSaber::Effects
