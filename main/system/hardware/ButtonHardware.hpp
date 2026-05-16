#pragma once
#include "GpioButton.hpp"
#include <memory>
#include "esp_err.h"

namespace InertialSaber::System::Hardware {
class ButtonHardware {
public:
    esp_err_t init();
    Espressif::Wrappers::GpioButton* getButton() { return m_button.get(); }
private:
    std::unique_ptr<Espressif::Wrappers::GpioButton> m_button;
};
}
