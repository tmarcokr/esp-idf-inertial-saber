#include "profiles/SoundFont.hpp"

#include "esp_log.h"
#include "esp_random.h"

#include <algorithm>

namespace InertialSaber::Profiles {

namespace {

constexpr const char* TAG = "SoundFont";
constexpr std::string_view kSdMountPoint = "/sdcard/";

constexpr std::string_view layoutFor(FontCategory category) {
    switch (category) {
    case FontCategory::Ignition:
        return "in/in";
    case FontCategory::Retraction:
        return "out/out";
    case FontCategory::Blaster:
        return "blst/blst";
    case FontCategory::Clash:
        return "clsh/clsh";
    case FontCategory::Drag:
        return "drag/drag";
    case FontCategory::DragEnd:
        return "enddrag/enddrag";
    case FontCategory::Burst:
        return "swng/swng";
    }
    return "";
}

constexpr size_t kLongestLeafLength = std::string_view("enddrag/enddrag255.wav").size();
static_assert(kSdMountPoint.size() + SoundFont::kMaxRootLength + kLongestLeafLength <=
              System::AudioPath::kMaxLength);

} // namespace

SoundFont::SoundFont(std::string_view rootPath, const Inertial::FontCounts& counts)
    : m_root(normalizeRoot(rootPath))
    , m_sdRoot(std::string(kSdMountPoint).append(m_root))
    , m_counts(counts) {
    if (m_root.empty()) {
        ESP_LOGW(TAG, "Empty font root '%.*s'; paths resolve to the SD mount point",
                 static_cast<int>(rootPath.size()), rootPath.data());
    } else if (m_root.size() > kMaxRootLength) {
        ESP_LOGE(TAG, "Font root '%s' exceeds %u characters; its sounds cannot play",
                 m_root.c_str(), static_cast<unsigned>(kMaxRootLength));
    }
}

std::string SoundFont::normalizeRoot(std::string_view rootPath) {
    const size_t first = rootPath.find_first_not_of('/');
    if (first == std::string_view::npos) {
        return {};
    }
    const size_t last = rootPath.find_last_not_of('/');
    std::string normalized(rootPath.substr(first, last - first + 1));
    normalized.push_back('/');
    return normalized;
}

const std::string& SoundFont::root() const {
    return m_root;
}

uint8_t SoundFont::count(FontCategory category) const {
    switch (category) {
    case FontCategory::Ignition:
        return m_counts.in;
    case FontCategory::Retraction:
        return m_counts.out;
    case FontCategory::Blaster:
        return m_counts.blaster;
    case FontCategory::Clash:
        return m_counts.clash;
    case FontCategory::Drag:
        return m_counts.drag;
    case FontCategory::DragEnd:
        return m_counts.dragEnd;
    case FontCategory::Burst:
        return m_counts.burst;
    }
    return 0;
}

uint8_t SoundFont::swingPairCount() const {
    return m_counts.swingPair;
}

System::AudioPath SoundFont::pathFor(FontCategory category, uint8_t index) const {
    return System::AudioPath(m_sdRoot)
        .append(layoutFor(category))
        .appendNumber(index)
        .append(".wav");
}

System::AudioPath SoundFont::randomPath(FontCategory category) const {
    const auto index = static_cast<uint8_t>(esp_random() % std::max<uint8_t>(count(category), 1));
    return pathFor(category, static_cast<uint8_t>(index + 1));
}

System::AudioPath SoundFont::humPath() const {
    return System::AudioPath(m_sdRoot).append("hum.wav");
}

System::AudioPath SoundFont::selectionPath() const {
    return System::AudioPath(m_sdRoot).append("font.wav");
}

System::AudioPath SoundFont::swingLowPath(uint8_t pairIndex) const {
    return System::AudioPath(m_sdRoot)
        .append("swingl/swingl")
        .appendNumber(pairIndex)
        .append(".wav");
}

System::AudioPath SoundFont::swingHighPath(uint8_t pairIndex) const {
    return System::AudioPath(m_sdRoot)
        .append("swingh/swingh")
        .appendNumber(pairIndex)
        .append(".wav");
}

} // namespace InertialSaber::Profiles
