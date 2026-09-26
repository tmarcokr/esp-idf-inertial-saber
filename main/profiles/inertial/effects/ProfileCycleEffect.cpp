#include "ProfileCycleEffect.hpp"
#include "profiles/ConfigurableProfile.hpp"
#include "profiles/ProfileManager.hpp"
#include "core/SaberDataPacket.hpp"
#include "esp_log.h"

namespace InertialSaber::Effects {

static constexpr const char* TAG = "ProfileCycle";

ProfileCycleEffect::ProfileCycleEffect(Profiles::ConfigurableProfile& profile,
                                       Profiles::ProfileManager& profileManager,
                                       uint8_t buttonId)
    : InertialEffect(1)
    , m_profile(profile)
    , m_profileManager(profileManager)
    , m_buttonId(buttonId)
{}

bool ProfileCycleEffect::test(const Core::SaberDataPacket& packet) {
    using PowerState = Profiles::ConfigurableProfile::PowerState;
    if (m_profile.getPowerState() != PowerState::RETRACTED) return false;
    if (m_buttonId >= Core::kMaxInputs) return false;

    const auto& input = packet.inputs[m_buttonId];
    using Gesture = Core::InputDescriptor::Gesture;
    return input.gesture == Gesture::Click && input.pressCount == 3;
}

void ProfileCycleEffect::run() {
    ESP_LOGI(TAG, "Profile cycle triggered");
    m_profileManager.next();
}

} // namespace InertialSaber::Effects
