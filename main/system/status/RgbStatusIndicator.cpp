#include "system/status/RgbStatusIndicator.hpp"

#include "esp_timer.h"

namespace InertialSaber::System::Status {

namespace {

using Espressif::Wrappers::Color;

constexpr Color kBootingColor{128, 128, 0};
constexpr Color kPreloadingColor{64, 64, 0};
constexpr Color kReadyColor{0, 32, 0};
constexpr Color kErrorColor{255, 0, 0};
constexpr Color kOffColor{0, 0, 0};
constexpr int64_t kBlinkHalfPeriodMs = 250;

constexpr bool isSameColor(const Color& a, const Color& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

} // namespace

RgbStatusIndicator::RgbStatusIndicator(const Config& config) : m_led(config.pin) {}

esp_err_t RgbStatusIndicator::init() {
    return m_led.init();
}

void RgbStatusIndicator::show(SystemStatus status) {
    write(colorFor(status));
}

Color RgbStatusIndicator::colorFor(SystemStatus status) {
    switch (status) {
    case SystemStatus::Booting:
        return kBootingColor;
    case SystemStatus::Preloading: {
        const bool blinkOn = ((esp_timer_get_time() / 1000LL) / kBlinkHalfPeriodMs) % 2 == 0;
        return blinkOn ? kPreloadingColor : kOffColor;
    }
    case SystemStatus::Ready:
        return kReadyColor;
    case SystemStatus::Error:
        return kErrorColor;
    }
    return kOffColor;
}

void RgbStatusIndicator::write(const Color& color) {
    if (m_hasLastColor && isSameColor(color, m_lastColor)) return;

    const esp_err_t err = isSameColor(color, kOffColor) ? m_led.clear() : m_led.setColor(color);
    if (err == ESP_OK) {
        m_lastColor = color;
        m_hasLastColor = true;
    }
}

} // namespace InertialSaber::System::Status
