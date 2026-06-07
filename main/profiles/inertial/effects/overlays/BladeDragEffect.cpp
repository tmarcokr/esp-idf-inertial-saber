#include "BladeDragEffect.hpp"
#include "SmartLedTypes.hpp"
#include "esp_random.h"

#include <algorithm>

namespace InertialSaber::Effects {

using namespace Espressif::Wrappers::SmartLed;

static constexpr uint32_t kFadeDurationMs = 150;

BladeDragEffect::BladeDragEffect(uint16_t numLeds, uint16_t dragLedCount)
    : m_numLeds(numLeds)
    , m_dragLedCount(dragLedCount) {}

void BladeDragEffect::update(uint32_t delta_ms) {
    if (m_fading) {
        m_fadeElapsed += delta_ms;
        if (m_fadeElapsed >= kFadeDurationMs) {
            m_fadeElapsed = kFadeDurationMs;
            m_finished = true;
        }
    }
}

void BladeDragEffect::render(Canvas& canvas) {
    if (isFinished()) {
        return;
    }

    float fadeScale = 1.0f;
    if (m_fading) {
        float progress = static_cast<float>(m_fadeElapsed) / kFadeDurationMs;
        fadeScale = 1.0f - progress;
    }

    uint16_t canvasSize = canvas.size();
    uint16_t startIdx = (m_numLeds > m_dragLedCount) ? (m_numLeds - m_dragLedCount) : 0;

    for (uint16_t i = startIdx; i < m_numLeds && i < canvasSize; ++i) {
        float factor = 1.0f;
        if (m_dragLedCount > 1) {
            factor = static_cast<float>(i - startIdx) / (m_dragLedCount - 1);
        }

        uint8_t flickerOffset = static_cast<uint8_t>(esp_random() % 51);
        uint8_t value = 255 - flickerOffset;
        uint16_t hue = 25 + static_cast<uint16_t>(20.0f * factor);
        uint8_t saturation = static_cast<uint8_t>(255.0f - (30.0f * factor));

        Color thermalColor = hsvToRgb(hue, saturation, value);
        uint8_t alpha = static_cast<uint8_t>(255.0f * factor * fadeScale);

        canvas.blendPixel(i, thermalColor, alpha);
    }
}

bool BladeDragEffect::isFinished() const {
    return m_finished;
}

void BladeDragEffect::terminate() {
    m_fading = true;
}

} // namespace InertialSaber::Effects
