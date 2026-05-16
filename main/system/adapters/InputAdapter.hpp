#pragma once

#include "GpioButton.hpp"
#include "core/bus/SaberActionBus.hpp"
#include "system/config/HardwareConfig.hpp"
#include "esp_err.h"

namespace InertialSaber::System::Adapters {

class InputAdapter {
public:
    explicit InputAdapter(Core::SaberActionBus& bus, Espressif::Wrappers::GpioButton& button);

    InputAdapter(const InputAdapter&) = delete;
    InputAdapter& operator=(const InputAdapter&) = delete;

    [[nodiscard]] esp_err_t start();

private:
    Core::SaberActionBus& m_bus;
    Espressif::Wrappers::GpioButton& m_mainButton;
    Core::InputDescriptor m_btnState{};
};

} // namespace InertialSaber::System::Adapters
