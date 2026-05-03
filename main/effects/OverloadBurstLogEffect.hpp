#pragma once

#include "InertialEffect.hpp"
#include "esp_log.h"

namespace InertialSaber::Effects {

/**
 * @brief Discrete log effect to test the TanqueOverload accumulator burst.
 * Prints a bright yellow warning to the serial monitor when triggered.
 */
class OverloadBurstLogEffect final : public Core::InertialEffect {
public:
    OverloadBurstLogEffect() {
        Priority = 1;
    }

    bool Test(const Core::SaberDataPacket& packet) override {
        return packet.OverloadBurst;
    }

    void Run() override {
        ESP_LOGW(TAG, "⚡ ===== OVERLOAD BURST TRIGGERED! ===== ⚡");
    }

private:
    static constexpr const char* TAG = "OverloadLog";
};

} // namespace InertialSaber::Effects
