// SPDX-License-Identifier: GPL-3.0-or-later

#include "EffectSet.hpp"

#include "esp_log.h"

#include <algorithm>
#include <utility>

namespace InertialSaber::Core {

static constexpr const char* TAG = "EffectSet";

EffectSet::EffectSet(const PhysicsConfig& physics, std::unique_ptr<EffectSetContext> context)
    : m_physics(physics)
    , m_context(std::move(context)) {
    m_effects.reserve(kMaxEffects);
}

bool EffectSet::add(std::unique_ptr<InertialEffect> effect) {
    if (!effect) {
        ESP_LOGE(TAG, "Effect rejected: null");
        return false;
    }
    if (m_effects.size() >= kMaxEffects) {
        ESP_LOGE(TAG, "Effect rejected: a set holds at most %u effects",
                 static_cast<unsigned>(kMaxEffects));
        return false;
    }
    const auto position = std::upper_bound(
        m_effects.begin(), m_effects.end(), effect->priority(),
        [](uint8_t priority, const auto& fx) { return priority < fx->priority(); });
    m_effects.insert(position, std::move(effect));
    return true;
}

} // namespace InertialSaber::Core
