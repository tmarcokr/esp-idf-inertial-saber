#pragma once

#include "Canvas.hpp"
#include "IEffect.hpp"

#include <atomic>
#include <cstdint>
#include <memory>

namespace InertialSaber::Effects {

/**
 * @brief Visual overlay effect for blade drag (friction burn).
 *
 * Renders a flickering orange/yellow thermal glow at the tip of the blade,
 * which fades out smoothly once the shared fade request is set.
 */
class BladeDragEffect final : public Espressif::Wrappers::SmartLed::IEffect {
public:
    /**
     * @brief Construct a new Blade Drag Effect.
     * @param numLeds Total number of LEDs in the blade.
     * @param dragLedCount Number of LEDs at the tip that show the thermal glow.
     * @param fadeRequest Set by the owner of the drag to start the fade-out; may be written from any task.
     */
    BladeDragEffect(uint16_t numLeds, uint16_t dragLedCount,
                    std::shared_ptr<const std::atomic<bool>> fadeRequest);

    void update(uint32_t deltaMs) override;
    void render(Espressif::Wrappers::SmartLed::Canvas& canvas) override;
    [[nodiscard]] bool isFinished() const override;

private:
    uint16_t m_numLeds;
    uint16_t m_dragLedCount;
    std::shared_ptr<const std::atomic<bool>> m_fadeRequest;
    bool m_fading = false;
    uint32_t m_fadeElapsed = 0;
    bool m_finished = false;
};

} // namespace InertialSaber::Effects
