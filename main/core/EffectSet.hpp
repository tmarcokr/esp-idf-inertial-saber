// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "InertialEffect.hpp"
#include "PhysicsConfig.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace InertialSaber::Core {

/**
 * @brief Opaque per-set state owned by an EffectSet and destroyed after its effects.
 */
class EffectSetContext {
public:
    virtual ~EffectSetContext() = default;
};

/**
 * @brief Priority-ordered list of effects plus the physics parameters they run with.
 *
 * Built and filled by one task, then handed to the bus (installEffects/stageEffects), after which
 * it is immutable. Destroyed by its owner off the bus.
 */
class EffectSet final {
public:
    static constexpr size_t kMaxEffects = 16;

    /**
     * @param physics Bus physics parameters applied when the set becomes active.
     * @param context Per-set state the effects may reference; may be null.
     */
    EffectSet(const PhysicsConfig& physics, std::unique_ptr<EffectSetContext> context);

    EffectSet(const EffectSet&) = delete;
    EffectSet& operator=(const EffectSet&) = delete;

    /**
     * @brief Inserts @p effect after any effect of equal priority. Builder task only.
     * @return false (error log) if @p effect is null or kMaxEffects effects are already held.
     */
    [[nodiscard]] bool add(std::unique_ptr<InertialEffect> effect);

    [[nodiscard]] const PhysicsConfig& physics() const { return m_physics; }

    /** @brief The effects in evaluation order. */
    [[nodiscard]] std::span<const std::unique_ptr<InertialEffect>> effects() const {
        return m_effects;
    }

private:
    PhysicsConfig m_physics;
    // Warning: declared before m_effects so the effects, which may reference the context, are
    // destroyed first.
    std::unique_ptr<EffectSetContext> m_context;
    std::vector<std::unique_ptr<InertialEffect>> m_effects;
};

} // namespace InertialSaber::Core
