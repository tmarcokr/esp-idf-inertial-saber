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

namespace InertialSaber::Profiles {

static constexpr const char* TAG = "ConfigurableProfile";

static constexpr size_t kVoicesPerProfile =
    Effects::InertialSwingEffect::kVoiceCount + Effects::DragEffect::kVoiceCount;
static constexpr size_t kVoiceMargin = 4;
static_assert(System::AudioController::kMaxVoices >= 2 * kVoicesPerProfile + kVoiceMargin);

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

void ConfigurableProfile::load(const SaberServices& services, ProfileManager& profileManager) {
    ESP_LOGD(TAG, "Loading configurable profile '%s'", m_def.profileName.c_str());

    m_power.handle(PowerStateMachine::Event::Lock);
    services.bus.setPhysicsConfig(m_def);

    services.audioCache.requestPreload(m_font);

    services.bus.registerEffect(std::make_unique<Effects::PreloadWaitEffect>(
        m_power, services.audioControl, services.audioCache, services.status, m_font));

    auto swingFx = std::make_unique<Effects::InertialSwingEffect>(services.audioControl, m_def,
                                                                  m_font, services.audioCache);
    m_swingEffect = swingFx.get();
    services.bus.registerEffect(std::move(swingFx));

    auto lightFx = std::make_unique<Effects::InertialLightEffect>(services.blade, m_def);
    m_lightEffect = lightFx.get();
    services.bus.registerEffect(std::move(lightFx));

    auto powerFx = std::make_unique<Effects::PowerToggleEffect>(
        m_power, *m_swingEffect, *m_lightEffect, services.audioControl, services.blade, m_def,
        m_font, Core::kMainButtonInputId);
    services.bus.registerEffect(std::move(powerFx));

    services.bus.registerEffect(std::make_unique<Effects::BlasterEffect>(
        m_power, services.audioControl, services.blade, m_def, m_font, Core::kMainButtonInputId));

    services.bus.registerEffect(std::make_unique<Effects::KineticImpactEffect>(
        m_power, services.audioControl, services.blade, m_def, m_font));

    services.bus.registerEffect(std::make_unique<Effects::DragEffect>(
        m_power, services.audioControl, services.blade, m_def, m_font, Core::kMainButtonInputId));

    services.bus.registerEffect(std::make_unique<Effects::ProfileCycleEffect>(
        m_power, profileManager, Core::kMainButtonInputId));
}

void ConfigurableProfile::unload(const SaberServices& services) {
    ESP_LOGD(TAG, "Unloading configurable profile '%s'", m_def.profileName.c_str());

    if (m_swingEffect) {
        m_swingEffect->deactivate();
    }
    if (m_lightEffect) {
        m_lightEffect->deactivate();
    }

    services.bus.clearEffects();

    m_swingEffect = nullptr;
    m_lightEffect = nullptr;
}

} // namespace InertialSaber::Profiles
