#pragma once

#include "Canvas.hpp"
#include "IEffect.hpp"
#include "SmartLedTypes.hpp"

#include <atomic>
#include <cstdint>

namespace InertialSaber::Effects {

/**
 * @brief SmartLed IEffect that renders HSB values received atomically from the
 * bus task. Acts as a lock-free bridge between InertialLightEffect (800Hz) and
 * the SmartLed Engine render loop (100 FPS).
 */
class InertialBladeEffect final : public Espressif::Wrappers::SmartLed::IEffect {
public:
    // Pack: [hue:16 | sat:8 | val:8] = 32 bits → lock-free on all ESP32 targets
    void setHSB(uint16_t hue, uint8_t sat, uint8_t val) {
        uint32_t packed = (uint32_t(hue) << 16) | (uint32_t(sat) << 8) | uint32_t(val);
        m_packedHsb.store(packed, std::memory_order_relaxed);
    }

    void update(uint32_t /*delta_ms*/) override {}

    void render(Espressif::Wrappers::SmartLed::Canvas &canvas) override {
        uint32_t packed = m_packedHsb.load(std::memory_order_relaxed);
        uint16_t h = static_cast<uint16_t>((packed >> 16) & 0xFFFF);
        uint8_t s = static_cast<uint8_t>((packed >> 8) & 0xFF);
        uint8_t v = static_cast<uint8_t>(packed & 0xFF);

        canvas.fill(Espressif::Wrappers::SmartLed::hsvToRgb(h, s, v));
    }

    [[nodiscard]] bool isFinished() const override { return false; }

private:
    std::atomic<uint32_t> m_packedHsb{0};
};

} // namespace InertialSaber::Effects
