#include "AudioPathProvider.hpp"
#include "esp_random.h"

namespace InertialSaber::Effects::InertialSwing {

AudioPathProvider::AudioPathProvider(const SwingFontConfig& config)
    : m_humPath(config.basePath + "/hum.wav"), m_config(config) {}

std::string AudioPathProvider::provideHumPath() const {
    return m_humPath;
}

SwingPathPair AudioPathProvider::provideSwingPaths() {
    if (m_config.swingPairCount > 1) {
        uint8_t newPair;
        do {
            newPair = randomInRange(m_config.swingPairCount);
        } while (newPair == m_currentPairIndex);
        m_currentPairIndex = newPair;
    } else {
        m_currentPairIndex = 0;
    }
    
    std::string prefix = m_config.basePath + "/swing";
    std::string suffix = std::to_string(m_currentPairIndex + 1) + ".wav";
    
    return { prefix + "l/swingl" + suffix, prefix + "h/swingh" + suffix };
}

std::string AudioPathProvider::provideBurstPath() {
    return m_config.basePath + "/swng/swng" + 
           std::to_string(randomInRange(m_config.burstCount) + 1) + ".wav";
}

uint8_t AudioPathProvider::randomInRange(uint8_t count) const {
    return count == 0 ? 0 : static_cast<uint8_t>(esp_random() % count);
}

} // namespace InertialSaber::Effects::InertialSwing
