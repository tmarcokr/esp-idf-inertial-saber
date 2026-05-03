#pragma once

#include "InertialEffect.hpp"
#include "esp_log.h"

namespace InertialSaber::Effects {

/**
 * @brief Discrete log effect to test the Inertial Overload accumulator burst.
 * Prints a bright yellow warning to the serial monitor when triggered.
 */
class InertialBurstLogEffect final : public Core::InertialEffect {
public:
    InertialBurstLogEffect() {
        Priority = 1;
    }

    bool Test(const Core::SaberDataPacket& packet) override {
        return packet.InertialBurst;
    }

    void Run() override {
        ESP_LOGW(TAG, "⚡ ===== INERTIAL BURST TRIGGERED! ===== ⚡");
    }

private:
    static constexpr const char* TAG = "InertialLog";
};

} // namespace InertialSaber::Effects
