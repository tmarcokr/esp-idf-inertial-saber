#include "BladeIgniteSweep.hpp"

#include <algorithm>

namespace InertialSaber::Effects {

BladeIgniteSweep::BladeIgniteSweep(uint16_t numLeds, uint16_t hue, uint32_t durationMs)
    : m_numLeds(numLeds)
    , m_color(Espressif::Wrappers::SmartLed::hsvToRgb(hue, 255u, 255u))
    , m_durationMs(durationMs > 0 ? durationMs : 1)
{}

void BladeIgniteSweep::update(uint32_t delta_ms) {
    m_elapsed = std::min(m_elapsed + delta_ms, m_durationMs);
}

void BladeIgniteSweep::render(Espressif::Wrappers::SmartLed::Canvas& canvas) {
    const uint16_t lit = static_cast<uint16_t>(
        (static_cast<uint64_t>(m_elapsed) * m_numLeds) / m_durationMs);

    canvas.fillRange(0, lit, m_color);
    if (lit < m_numLeds) {
        canvas.fillRange(lit, static_cast<uint16_t>(m_numLeds - lit),
                         Espressif::Wrappers::SmartLed::Color::Off());
    }
}

bool BladeIgniteSweep::isFinished() const {
    return m_elapsed >= m_durationMs;
}

} // namespace InertialSaber::Effects
