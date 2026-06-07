#include "BladeBlasterBlock.hpp"
#include "esp_random.h"
#include <algorithm>

namespace InertialSaber::Effects {

BladeBlasterBlock::BladeBlasterBlock(uint16_t numLeds, uint16_t ledCount, uint32_t durationMs)
    : m_numLeds(numLeds)
    , m_ledCount(std::min(ledCount, numLeds))
    , m_durationMs(durationMs > 0 ? durationMs : 1)
{
    const uint16_t max_start = static_cast<uint16_t>(m_numLeds - m_ledCount);
    m_startLed = (max_start > 0) ? static_cast<uint16_t>(esp_random() % (max_start + 1)) : 0;
}

void BladeBlasterBlock::update(uint32_t delta_ms) {
    m_elapsed = std::min(m_elapsed + delta_ms, m_durationMs);
}

void BladeBlasterBlock::render(Espressif::Wrappers::SmartLed::Canvas& canvas) {
    if (!isFinished()) {
        canvas.fillRange(m_startLed, m_ledCount, Espressif::Wrappers::SmartLed::Color::White());
    }
}

bool BladeBlasterBlock::isFinished() const {
    return m_elapsed >= m_durationMs;
}

} // namespace InertialSaber::Effects
