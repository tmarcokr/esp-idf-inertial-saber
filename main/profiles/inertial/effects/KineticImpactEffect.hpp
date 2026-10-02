#pragma once

#include "core/InertialEffect.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace InertialSaber::Profiles {
class PowerStateMachine;
class SoundFont;
} // namespace InertialSaber::Profiles
namespace InertialSaber::Profiles::Inertial {
struct InertialDefinition;
}
namespace InertialSaber::Core {
struct SaberDataPacket;
}
namespace InertialSaber::System {
class AudioController;
}
namespace Espressif::Wrappers::SmartLed {
class Engine;
}

namespace InertialSaber::Effects {

/**
 * @brief Evaluates physical impact triggers (clash) and plays audio + LED flash overlays.
 */
class KineticImpactEffect final : public Core::InertialEffect {
public:
    KineticImpactEffect(const Profiles::PowerStateMachine& power, System::AudioController& audio,
                        Espressif::Wrappers::SmartLed::Engine& ledEngine,
                        const Profiles::Inertial::InertialDefinition& definition,
                        const Profiles::SoundFont& font);

    bool test(const Core::SaberDataPacket& packet) override;
    void run() override;

private:
    static constexpr uint32_t kClashWindowMs = 15;
    static constexpr int64_t kClashWindowUs = static_cast<int64_t>(kClashWindowMs) * 1000;
    static constexpr size_t kKineticEnergyHistorySize = 16;
    static constexpr uint32_t kClashDebounceMs = 500;
    static constexpr uint32_t kClashRetriggerWindowMs = 1000;
    static constexpr uint32_t kMaxImuRateHz = 1000;
    static_assert(kKineticEnergyHistorySize >= kClashWindowMs * kMaxImuRateHz / 1000 + 1,
                  "Clash history ring cannot hold a full window at the maximum IMU rate");

    struct KineticEnergySample {
        int64_t timestampUs;
        float kineticEnergyG;
    };

    void clearKineticEnergyHistory();
    void recordKineticEnergy(int64_t timestampUs, float kineticEnergyG);
    [[nodiscard]] float peakKineticEnergySince(int64_t oldestTimestampUs) const;
    bool detectClash(const Core::SaberDataPacket& packet);

    const Profiles::PowerStateMachine& m_power;
    System::AudioController& m_audio;
    Espressif::Wrappers::SmartLed::Engine& m_ledEngine;
    const Profiles::Inertial::InertialDefinition& m_def;
    const Profiles::SoundFont& m_font;

    std::array<KineticEnergySample, kKineticEnergyHistorySize> m_kineticEnergyHistory{};
    size_t m_historyNext = 0;
    size_t m_historySize = 0;
    int64_t m_lastSampleTimestampUs = 0;
    uint32_t m_lastClashTimeMs = 0;
    uint32_t m_previousClashTimeMs = 0;
};

} // namespace InertialSaber::Effects
