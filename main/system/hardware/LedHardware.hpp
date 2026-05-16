#pragma once
#include "Engine.hpp"
#include <memory>
#include "esp_err.h"

namespace InertialSaber::System::Hardware {
class LedHardware {
public:
    esp_err_t init();
    Espressif::Wrappers::SmartLed::Engine* getEngine() { return m_ledEngine.get(); }
private:
    std::unique_ptr<Espressif::Wrappers::SmartLed::Engine> m_ledEngine;
};
}
