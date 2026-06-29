#include "ProfileCycleEffect.hpp"
#include "profiles/ConfigurableProfile.hpp"
#include "profiles/ProfileManager.hpp"
#include "system/hardware/HardwareConfig.hpp"
#include "core/SaberDataPacket.hpp"
#include "esp_log.h"
#include <string>

namespace InertialSaber::Effects {

static constexpr const char* TAG = "ProfileCycle";

ProfileCycleEffect::ProfileCycleEffect(
    Profiles::ConfigurableProfile&           profile,
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
    using PowerState = Profiles::ConfigurableProfile::PowerState;
    if (m_profile.getPowerState() != PowerState::RETRACTED) return false;
    if (m_buttonId >= System::Hardware::HardwareConfig::kMaxInputs) return false;

    const auto& input = packet.inputs[m_buttonId];
    using Gesture = Core::InputDescriptor::Gesture;
    return input.gesture == Gesture::CLICK && input.pressCount == 3;
}

void ProfileCycleEffect::Run() {
    ESP_LOGI(TAG, "Profile cycle triggered");
    m_profileManager.nextProfile(m_bus, m_audio, m_led);
}

} // namespace InertialSaber::Effects
