// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <type_traits>

namespace InertialSaber::System {

/**
 * @brief Fixed-capacity, null-terminated audio file path built without heap allocation.
 *
 * An append that does not fit leaves the content unchanged and marks the path as overflowed;
 * ok() then stays false. Trivially copyable, so it can travel through FreeRTOS queues.
 */
class AudioPath {
public:
    /** @brief Buffer size in bytes, including the null terminator. */
    static constexpr size_t kCapacity = 128;
    /** @brief Longest storable path in characters. */
    static constexpr size_t kMaxLength = kCapacity - 1;

    constexpr AudioPath() = default;

    explicit AudioPath(std::string_view text) { append(text); }

    AudioPath& append(std::string_view text) {
        if (m_overflow || text.size() > kMaxLength - m_length) {
            m_overflow = true;
            return *this;
        }
        if (text.empty()) return *this;
        std::memcpy(m_buffer.data() + m_length, text.data(), text.size());
        m_length = static_cast<uint8_t>(m_length + text.size());
        m_buffer[m_length] = '\0';
        return *this;
    }

    /** @brief Appends the decimal representation of @p value. */
    AudioPath& appendNumber(uint32_t value) {
        std::array<char, std::numeric_limits<uint32_t>::digits10 + 1> digits{};
        const auto result = std::to_chars(digits.data(), digits.data() + digits.size(), value);
        return append({digits.data(), static_cast<size_t>(result.ptr - digits.data())});
    }

    [[nodiscard]] std::string_view view() const { return {m_buffer.data(), m_length}; }
    [[nodiscard]] const char* c_str() const { return m_buffer.data(); }
    [[nodiscard]] bool empty() const { return m_length == 0; }

    /** @brief True when no append has overflowed the capacity. */
    [[nodiscard]] bool ok() const { return !m_overflow; }

private:
    std::array<char, kCapacity> m_buffer{};
    uint8_t m_length = 0;
    bool m_overflow = false;
};

static_assert(AudioPath::kMaxLength <= UINT8_MAX);
static_assert(std::is_trivially_copyable_v<AudioPath>);

} // namespace InertialSaber::System
