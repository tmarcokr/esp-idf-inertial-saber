#pragma once

#include "AudioEngine.hpp"
#include "Engine.hpp"
#include "InertialDefinition.hpp"
#include "SaberActionBus.hpp"

namespace InertialSaber::Effects {
    class InertialSwingEffect;
    class InertialLightEffect;
} // namespace InertialSaber::Effects

namespace InertialSaber::Core {

/**
 * @brief Abstract contract for a saber personality profile.
 *
 * A concrete profile supplies its InertialDefinition via getDefinition() and
 * implements load() / unload() to register and deregister its InertialEffect
 * objects on the SaberAction Bus.
 *
 * Hot-swap lifecycle (caller responsibility):
 *   1. current->unload(bus)   — deactivate effects, then clear bus.
 *   2. next->load(bus, ...)   — instantiate effects, register on bus.
 *
 * The non-owning pointers swingEffect / lightEffect are populated during load()
 * to allow cross-wired effects (e.g. PowerToggleEffect) to reference engine
 * effects without coupling the caller to concrete types.
 */
class InertialProfile {
public:
    virtual ~InertialProfile() = default;

    /**
     * @brief Returns the static definition (physics, audio, visual config) for this profile.
     */
    [[nodiscard]] virtual const InertialDefinition& getDefinition() const = 0;

    /**
     * @brief Instantiate and register this profile's effects on the bus.
     * @param bus   The active SaberAction Bus.
     * @param audio The AudioEngine instance (owned by SaberSystem).
     * @param led   The SmartLed Engine instance (owned by SaberSystem).
     */
    virtual void load(SaberActionBus& bus,
                      Espressif::Wrappers::Audio::AudioEngine& audio,
                      Espressif::Wrappers::SmartLed::Engine& led) = 0;

    /**
     * @brief Deactivate this profile's effects and clear them from the bus.
     *
     * Concrete implementations must call deactivate() on each engine effect
     * before invoking bus.clearEffects(), ensuring a clean audio/visual shutdown.
     */
    virtual void unload(SaberActionBus& bus) = 0;

    // Non-owning pointers to cross-wired engine effects; valid only between
    // load() and unload(). Null otherwise.
    Effects::InertialSwingEffect* swingEffect = nullptr;
    Effects::InertialLightEffect* lightEffect = nullptr;
};

} // namespace InertialSaber::Core
