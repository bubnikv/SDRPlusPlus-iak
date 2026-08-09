#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace color_utils {
    namespace detail {
        inline int hexDigit(char value) noexcept {
            if (value >= '0' && value <= '9') { return value - '0'; }
            if (value >= 'A' && value <= 'F') { return value - 'A' + 10; }
            if (value >= 'a' && value <= 'f') { return value - 'a' + 10; }
            return -1;
        }
    }

    template<std::size_t ChannelCount>
    std::optional<std::array<uint8_t, ChannelCount>> parseHex(
        std::string_view value) noexcept
    {
        static_assert(ChannelCount > 0, "a color must contain at least one channel");
        if (value.size() != 1 + (ChannelCount * 2) || value.front() != '#') {
            return std::nullopt;
        }

        std::array<uint8_t, ChannelCount> channels{};
        for (std::size_t i = 0; i < ChannelCount; i++) {
            const int high = detail::hexDigit(value[1 + (i * 2)]);
            const int low = detail::hexDigit(value[2 + (i * 2)]);
            if (high < 0 || low < 0) { return std::nullopt; }
            channels[i] = static_cast<uint8_t>((high << 4) | low);
        }
        return channels;
    }
}
