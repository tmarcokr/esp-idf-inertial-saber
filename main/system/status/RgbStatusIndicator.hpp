#pragma once

#include "system/status/StatusIndicator.hpp"
#include "RgbLed.hpp"

#include "driver/gpio.h"
#include "sdkconfig.h"

#if CONFIG_SABER_METRICS
#include <mutex>
#endif

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
#if CONFIG_SABER_METRICS
    void showActivity(ActivitySignal signal) override;
#endif

private:
    static Espressif::Wrappers::Color colorFor(SystemStatus status);
    void write(const Espressif::Wrappers::Color& color);

    Espressif::Wrappers::RgbLed m_led;
    Espressif::Wrappers::Color m_lastColor{};
    bool m_hasLastColor = false;

#if CONFIG_SABER_METRICS
    static Espressif::Wrappers::Color colorFor(ActivitySignal signal);

    std::mutex m_mutex;
    SystemStatus m_status = SystemStatus::Booting;
    ActivitySignal m_activity = ActivitySignal::None;
#endif
};

} // namespace InertialSaber::System::Status
