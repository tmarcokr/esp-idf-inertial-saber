#pragma once

#include "InertialEffect.hpp"

#include "esp_log.h"
#include "esp_timer.h"

namespace InertialSaber::Effects {

/**
 * @brief Flow Modulator that prints IMU metrics to the serial monitor.
 *
 * Priority 0 (Background). Always active, but throttled to avoid
 * flooding the output. Useful for validating that motion data flows
 * through the SaberAction Bus correctly.
 */
class MotionLogEffect final : public Core::InertialEffect {
public:
    explicit MotionLogEffect(uint32_t interval_ms = 500) : m_intervalUs(interval_ms * 1000) {
        Priority = 0;
    }

    bool Test(const Core::SaberDataPacket& packet) override {
        int64_t now = esp_timer_get_time();
        if ((now - m_lastLogUs) < m_intervalUs) {
            return false;
        }
        m_lastLogUs = now;
        m_cachedPacket = packet;
        return true;
    }

    void Run() override {
        ESP_LOGI(TAG, "E=%.2fG  R=[%.0f, %.0f, %.0f]  A=%.2f  OL=%.2f B=%d",
                 m_cachedPacket.KineticEnergy,
                 m_cachedPacket.AxisRotation[0],
                 m_cachedPacket.AxisRotation[1],
                 m_cachedPacket.AxisRotation[2],
                 m_cachedPacket.OrientationVector,
                 m_cachedPacket.TanqueOverload,
                 m_cachedPacket.OverloadBurst);
    }

private:
    static constexpr const char* TAG = "MotionLog";
    int64_t m_intervalUs;
    int64_t m_lastLogUs = 0;
    Core::SaberDataPacket m_cachedPacket{};
};

} // namespace InertialSaber::Effects
