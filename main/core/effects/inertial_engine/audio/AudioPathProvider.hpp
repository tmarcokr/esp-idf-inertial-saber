#pragma once

#include <cstdint>
#include <string>

namespace InertialSaber::Effects::InertialSwing {

/**
 * @brief Configuration for sound font file paths and counts.
 */
struct SwingFontConfig {
    std::string basePath;
    uint8_t humCount;
    uint8_t swingPairCount;
    uint8_t burstCount;
};

/**
 * @brief A cohesive pair of paths for low and high swing sounds.
 */
struct SwingPathPair {
    std::string low;
    std::string high;
};

/**
 * @brief Manages the state and resolution of audio paths for the InertialSwing engine.
 */
class AudioPathProvider {
public:
    /**
     * @brief Constructs the provider with a specific font configuration.
     * @param config The sound font configuration.
     */
    explicit AudioPathProvider(const SwingFontConfig& config);

    /**
     * @brief Provides the base hum loop path.
     * @return Absolute VFS path to the hum audio file.
     */
    [[nodiscard]] std::string provideHumPath() const;

    /**
     * @brief Generates and provides a new randomized pair of swing paths.
     * @return A valid SwingPathPair containing matched low and high audio paths.
     */
    [[nodiscard]] SwingPathPair provideSwingPaths();

    /**
     * @brief Generates and provides a new randomized burst path.
     * @return Absolute VFS path to a burst audio file.
     */
    [[nodiscard]] std::string provideBurstPath();

    /**
     * @brief Retrieves the active font configuration.
     * @return Reference to the internal SwingFontConfig.
     */
    [[nodiscard]] const SwingFontConfig& getConfig() const { return m_config; }

    /**
     * @brief Retrieves the index of the currently active swing pair.
     * @return The 0-based index of the active pair.
     */
    [[nodiscard]] uint8_t getCurrentPairIndex() const { return m_currentPairIndex; }

private:
    const std::string m_humPath;
    SwingFontConfig m_config;
    uint8_t m_currentPairIndex = 255;
    
    [[nodiscard]] uint8_t randomInRange(uint8_t count) const;
};

} // namespace InertialSaber::Effects::InertialSwing
