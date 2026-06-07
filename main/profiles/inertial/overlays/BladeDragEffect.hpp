#pragma once

#include "Canvas.hpp"
#include "IEffect.hpp"

#include <cstdint>

namespace InertialSaber::Effects {

/**
 * @brief Visual overlay effect for blade drag (friction burn).
 *
 * Renders a flickering orange/yellow thermal glow at the tip of the blade,
 * which fades out smoothly when the drag interaction is terminated.
 */
class BladeDragEffect final : public Espressif::Wrappers::SmartLed::IEffect {
public:
    /**
     * @brief Construct a new Blade Drag Effect.
     * @param numLeds Total number of LEDs in the blade.
     * @param dragLedCount Number of LEDs at the tip that show the thermal glow.
     */
    BladeDragEffect(uint16_t numLeds, uint16_t dragLedCount);

    void update(uint32_t delta_ms) override;
    void render(Espressif::Wrappers::SmartLed::Canvas& canvas) override;
    [[nodiscard]] bool isFinished() const override;

    /**
     * @brief Terminate the drag effect and initiate the smooth fade-out phase.
     */
    void terminate();

private:
    uint16_t m_numLeds;
    uint16_t m_dragLedCount;
    bool m_fading = false;
    uint32_t m_fadeElapsed = 0;
    bool m_finished = false;
};

} // namespace InertialSaber::Effects
