#include "BladeClashFlash.hpp"
#include <algorithm>

namespace InertialSaber::Effects {

using namespace Espressif::Wrappers::SmartLed;

BladeClashFlash::BladeClashFlash(uint16_t numLeds, uint16_t baseHue, uint32_t durationMs)
    : m_numLeds(numLeds)
    , m_clashHue((baseHue + 180) % 360)
    , m_durationMs(durationMs > 0 ? durationMs : 1) {}

void BladeClashFlash::update(uint32_t delta_ms) {
    m_elapsed = std::min(m_elapsed + delta_ms, m_durationMs);
}

void BladeClashFlash::render(Canvas& canvas) {
    if (isFinished()) {
        return;
    }

    float progress = static_cast<float>(m_elapsed) / m_durationMs;
    uint8_t alpha = static_cast<uint8_t>(255.0f * (1.0f - progress));
    Color clashColor = hsvToRgb(m_clashHue, 255, 255);

    uint16_t n = std::min(m_numLeds, canvas.size());
    for (uint16_t i = 0; i < n; ++i) {
        canvas.blendPixel(i, clashColor, alpha);
    }
}

bool BladeClashFlash::isFinished() const {
    return m_elapsed >= m_durationMs;
}

} // namespace InertialSaber::Effects
