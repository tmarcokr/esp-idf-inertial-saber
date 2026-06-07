#include "ProfileCycleEffect.hpp"
#include "models/SaberDataPacket.hpp"
#include "esp_log.h"

namespace InertialSaber::Effects {

static constexpr const char* TAG = "ProfileCycle";

ProfileCycleEffect::ProfileCycleEffect(
    Core::InertialProfile&                   profile,
    Profiles::ProfileManager&                profileManager,
    Core::SaberActionBus&                    bus,
    Espressif::Wrappers::Audio::AudioEngine& audio,
    Espressif::Wrappers::SmartLed::Engine&   led,
    uint8_t                                  buttonId)
    : m_profile(profile)
    , m_profileManager(profileManager)
    , m_bus(bus)
    , m_audio(audio)
    , m_led(led)
    , m_buttonId(buttonId)
{
    Priority = 1;
}

bool ProfileCycleEffect::Test(const Core::SaberDataPacket& packet) {
    using PowerState = Core::InertialProfile::PowerState;
    if (m_profile.getPowerState() != PowerState::IGNITED) return false;
    if (m_buttonId >= Core::Platform::kMaxInputs)         return false;

    const auto& input = packet.inputs[m_buttonId];
    using Gesture = Core::InputDescriptor::Gesture;
    return input.gesture == Gesture::CLICK && input.pressCount == 3;
}

void ProfileCycleEffect::Run() {
    ESP_LOGI(TAG, "Profile cycle triggered");
    m_profileManager.nextProfile(m_bus, m_audio, m_led);
}

} // namespace InertialSaber::Effects
