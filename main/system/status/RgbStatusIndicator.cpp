// SPDX-License-Identifier: GPL-3.0-or-later

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
#if CONFIG_SABER_METRICS
constexpr Color kWriteColor{0, 0, 64};
constexpr Color kWriteFailedColor = kErrorColor;
constexpr int64_t kActivityBlinkHalfPeriodMs = 100;
#endif

constexpr bool isSameColor(const Color& a, const Color& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

Color blink(const Color& color, int64_t halfPeriodMs) {
    const bool blinkOn = ((esp_timer_get_time() / 1000LL) / halfPeriodMs) % 2 == 0;
    return blinkOn ? color : kOffColor;
}

} // namespace

RgbStatusIndicator::RgbStatusIndicator(const Config& config) : m_led(config.pin) {}

esp_err_t RgbStatusIndicator::init() {
    return m_led.init();
}

void RgbStatusIndicator::show(SystemStatus status) {
#if CONFIG_SABER_METRICS
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_status = status;
    if (m_activity != ActivitySignal::None) return;
#endif
    write(colorFor(status));
}

#if CONFIG_SABER_METRICS
void RgbStatusIndicator::showActivity(ActivitySignal signal) {
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_activity = signal;
    write(signal == ActivitySignal::None ? colorFor(m_status) : colorFor(signal));
}

Color RgbStatusIndicator::colorFor(ActivitySignal signal) {
    switch (signal) {
    case ActivitySignal::None:
        return kOffColor;
    case ActivitySignal::Writing:
        return kWriteColor;
    case ActivitySignal::Written:
        return blink(kWriteColor, kActivityBlinkHalfPeriodMs);
    case ActivitySignal::WriteFailed:
        return blink(kWriteFailedColor, kActivityBlinkHalfPeriodMs);
    }
    return kOffColor;
}
#endif

Color RgbStatusIndicator::colorFor(SystemStatus status) {
    switch (status) {
    case SystemStatus::Booting:
        return kBootingColor;
    case SystemStatus::Preloading:
        return blink(kPreloadingColor, kBlinkHalfPeriodMs);
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
