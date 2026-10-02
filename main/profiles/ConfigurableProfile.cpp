#include "profiles/ConfigurableProfile.hpp"
#include "profiles/ProfileParser.hpp"
#include "profiles/ProfileManager.hpp"
#include "InertialLightEffect.hpp"
#include "PowerToggleEffect.hpp"
#include "InertialSwingEffect.hpp"
#include "BlasterEffect.hpp"
#include "KineticImpactEffect.hpp"
#include "DragEffect.hpp"
#include "profiles/inertial/effects/ProfileCycleEffect.hpp"
#include "profiles/inertial/effects/PreloadWaitEffect.hpp"
#include "core/SaberDataPacket.hpp"

#include "esp_log.h"

#include <utility>

namespace InertialSaber::Profiles {

namespace {

constexpr const char* TAG = "ConfigurableProfile";

constexpr size_t kEffectsPerProfile = 8;
static_assert(kEffectsPerProfile <= Core::EffectSet::kMaxEffects);

constexpr size_t kVoicesPerProfile =
    Effects::InertialSwingEffect::kVoiceCount + Effects::DragEffect::kVoiceCount;
constexpr size_t kVoiceMargin = 4;
static_assert(System::AudioController::kMaxVoices >= 2 * kVoicesPerProfile + kVoiceMargin);

struct ProfileContext final : Core::EffectSetContext {
    PowerStateMachine power;
};

} // namespace

std::unique_ptr<ConfigurableProfile> ConfigurableProfile::fromJson(std::string_view json) {
    Inertial::InertialDefinition definition{};
    if (ProfileParser::parse(json, definition) != ESP_OK) {
        return nullptr;
    }
    return std::make_unique<ConfigurableProfile>(std::move(definition));
}

ConfigurableProfile::ConfigurableProfile(Inertial::InertialDefinition definition)
    : m_def(std::move(definition))
    , m_font(m_def.profileRoot, m_def.fontCounts) {}

const Inertial::InertialDefinition& ConfigurableProfile::definition() const {
    return m_def;
}

const SoundFont& ConfigurableProfile::font() const {
    return m_font;
}

ProfileEffects ConfigurableProfile::buildEffects(const SaberServices& services,
                                                 ProfileManager& profileManager,
                                                 std::optional<uint32_t> switchRequestedUs) const {
    ESP_LOGD(TAG, "Building effects of profile '%s'", m_def.profileName.c_str());

    auto context = std::make_unique<ProfileContext>();
    PowerStateMachine& power = context->power;
    auto set = std::make_unique<Core::EffectSet>(m_def, std::move(context));

    auto swingFx =
        std::make_unique<Effects::InertialSwingEffect>(services.audioControl, m_def, m_font);
    auto lightFx = std::make_unique<Effects::InertialLightEffect>(services.blade, m_def);
    auto powerFx = std::make_unique<Effects::PowerToggleEffect>(
        power, *swingFx, *lightFx, services.audioControl, services.blade, m_def, m_font,
        Core::kMainButtonInputId);

    const bool complete =
        set->add(std::make_unique<Effects::PreloadWaitEffect>(power, services.audioControl,
                                                              services.audioCache, services.status,
                                                              m_font, switchRequestedUs)) &&
        set->add(std::move(swingFx)) && set->add(std::move(lightFx)) &&
        set->add(std::move(powerFx)) &&
        set->add(std::make_unique<Effects::BlasterEffect>(power, services.audioControl,
                                                          services.blade, m_def, m_font,
                                                          Core::kMainButtonInputId)) &&
        set->add(std::make_unique<Effects::KineticImpactEffect>(power, services.audioControl,
                                                                services.blade, m_def, m_font)) &&
        set->add(std::make_unique<Effects::DragEffect>(power, services.audioControl, services.blade,
                                                       m_def, m_font, Core::kMainButtonInputId)) &&
        set->add(std::make_unique<Effects::ProfileCycleEffect>(power, profileManager,
                                                               Core::kMainButtonInputId));
    if (!complete) {
        return {};
    }
    return {std::move(set), &power};
}

} // namespace InertialSaber::Profiles
