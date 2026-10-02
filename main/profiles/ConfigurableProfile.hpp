#pragma once

#include "core/EffectSet.hpp"
#include "profiles/inertial/InertialDefinition.hpp"
#include "profiles/PowerStateMachine.hpp"
#include "profiles/SaberServices.hpp"
#include "profiles/SoundFont.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

namespace InertialSaber::Profiles {

class ProfileManager;

/**
 * @brief Effect set built for one profile, plus the power state machine its effects share.
 */
struct ProfileEffects {
    std::unique_ptr<Core::EffectSet> set;
    /** @brief Owned by the context of @ref set; valid while the set is alive. */
    const PowerStateMachine* power = nullptr;
};

/**
 * @brief An immutable profile driven by an InertialDefinition structure.
 *
 * Builds independent effect sets for the core effects suite; each set owns its own power state
 * machine. Thread-safe for concurrent const access.
 */
class ConfigurableProfile final {
public:
    /**
     * @brief Parses a profile.json document once and builds the profile from it.
     * @return The profile, or nullptr if the JSON cannot be parsed.
     */
    [[nodiscard]] static std::unique_ptr<ConfigurableProfile> fromJson(std::string_view json);

    explicit ConfigurableProfile(Inertial::InertialDefinition definition);

    ConfigurableProfile(const ConfigurableProfile&) = delete;
    ConfigurableProfile& operator=(const ConfigurableProfile&) = delete;
    ConfigurableProfile(ConfigurableProfile&&) = delete;
    ConfigurableProfile& operator=(ConfigurableProfile&&) = delete;

    [[nodiscard]] const Inertial::InertialDefinition& definition() const;
    [[nodiscard]] const SoundFont& font() const;

    /**
     * @brief Builds a fresh effect set for this profile with its own PowerStateMachine (starts Locked).
     *
     * Callable from main (bus stopped) or profile_ctrl; touches neither the bus nor the LED engine.
     * @param services Services wired into the effects.
     * @param profileManager Receiver of the profile cycle requests of the set.
     * @param switchRequestedUs Time of the switch request (esp_timer, µs, truncated to 32 bits);
     *        empty at boot.
     * @return The set, or a null set if an effect was rejected.
     */
    [[nodiscard]] ProfileEffects buildEffects(const SaberServices& services,
                                              ProfileManager& profileManager,
                                              std::optional<uint32_t> switchRequestedUs) const;

private:
    Inertial::InertialDefinition m_def;
    SoundFont m_font;
};

} // namespace InertialSaber::Profiles
