#pragma once
#include "AudioEngine.hpp"
#include <memory>
#include "esp_err.h"

namespace InertialSaber::System::Hardware {
class AudioHardware {
public:
    esp_err_t init();
    Espressif::Wrappers::Audio::AudioEngine* getEngine() { return m_audioEngine.get(); }
private:
    std::unique_ptr<Espressif::Wrappers::Audio::AudioEngine> m_audioEngine;
};
}
