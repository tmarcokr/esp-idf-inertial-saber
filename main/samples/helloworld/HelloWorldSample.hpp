#pragma once

#include "ISample.hpp"

namespace Espressif::App {

class HelloWorldSample : public ISample {
public:
    esp_err_t setup() override;
    void run() override;
};

} // namespace Espressif::App
