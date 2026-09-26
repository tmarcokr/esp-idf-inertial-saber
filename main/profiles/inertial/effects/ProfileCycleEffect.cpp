#include "ProfileCycleEffect.hpp"
#include "profiles/PowerStateMachine.hpp"
#include "profiles/ProfileManager.hpp"
#include "core/SaberDataPacket.hpp"
#include "esp_log.h"

namespace InertialSaber::Effects {

static constexpr const char* TAG = "ProfileCycle";

ProfileCycleEffect::ProfileCycleEffect(const Profiles::PowerStateMachine& power,
                                       Profiles::ProfileManager& profileManager,
                                       uint8_t buttonId)
    : InertialEffect(1)
    , m_power(power)
    , m_profileManager(profileManager)
    , m_buttonId(buttonId)
{}

bool ProfileCycleEffect::test(const Core::SaberDataPacket& packet) {
    if (!m_power.isRetracted()) return false;
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
