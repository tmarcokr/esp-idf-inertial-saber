#pragma once

#include "system/status/StatusIndicator.hpp"
#include "RgbLed.hpp"

#include "driver/gpio.h"

namespace InertialSaber::System::Status {

/**
 * @brief StatusIndicator backed by a single addressable RGB LED.
 */
class RgbStatusIndicator final : public StatusIndicator {
public:
    /**
     * @brief Hardware parameters of the indicator.
     */
    struct Config {
        gpio_num_t pin;
    };

    explicit RgbStatusIndicator(const Config& config);

    RgbStatusIndicator(const RgbStatusIndicator&) = delete;
    RgbStatusIndicator& operator=(const RgbStatusIndicator&) = delete;

    [[nodiscard]] esp_err_t init() override;
    void show(SystemStatus status) override;

private:
    static Espressif::Wrappers::Color colorFor(SystemStatus status);
    void write(const Espressif::Wrappers::Color& color);

    Espressif::Wrappers::RgbLed m_led;
    Espressif::Wrappers::Color m_lastColor{};
    bool m_hasLastColor = false;
};

} // namespace InertialSaber::System::Status
