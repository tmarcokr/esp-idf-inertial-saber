#pragma once
#include "sd_card.hpp"
#include <memory>
#include "esp_err.h"

namespace InertialSaber::System::Hardware {
class SdHardware {
public:
    esp_err_t init();
private:
    std::unique_ptr<Espressif::Wrappers::SdCard> m_sdCard;
};
}
