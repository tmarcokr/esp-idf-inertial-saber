#pragma once

#include "profiles/inertial/InertialDefinition.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace InertialSaber::Profiles {

/** @brief Randomly selectable one-shot/loop sound categories of a font. */
enum class FontCategory : uint8_t { Ignition, Retraction, Blaster, Clash, Drag, DragEnd, Burst };

/**
 * @brief Immutable description of a profile's sound font on the SD card.
 *
 * The root is normalized once (no leading '/', exactly one trailing '/'), so every
 * path has single separators. Thread-safe for concurrent const access.
 */
class SoundFont {
public:
    SoundFont(std::string_view rootPath, const Inertial::FontCounts& counts);

    /** @brief Returns the root without leading '/' and with exactly one trailing '/' ("" for an empty root). */
    [[nodiscard]] static std::string normalizeRoot(std::string_view rootPath);

    /** @brief Normalized root relative to the SD mount point (e.g. "profiles/x/"). */
    [[nodiscard]] const std::string& root() const;

    [[nodiscard]] uint8_t count(FontCategory category) const;
    [[nodiscard]] uint8_t swingPairCount() const;

    /** @brief Absolute SD path of the file with the 1-based @p index in @p category. */
    [[nodiscard]] std::string pathFor(FontCategory category, uint8_t index) const;

    /** @brief Absolute SD path of a uniformly random file in @p category (index 1 when the count is 0). */
    [[nodiscard]] std::string randomPath(FontCategory category) const;

    [[nodiscard]] std::string humPath() const;
    [[nodiscard]] std::string selectionPath() const;
    [[nodiscard]] std::string swingLowPath(uint8_t pairIndex) const;
    [[nodiscard]] std::string swingHighPath(uint8_t pairIndex) const;

private:
    std::string m_root;
    std::string m_sdRoot;
    Inertial::FontCounts m_counts;
};

} // namespace InertialSaber::Profiles
