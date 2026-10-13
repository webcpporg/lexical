// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Strict UTF-8 decoding (RFC 3629), lexical's own: no library reaches into another's detail.

#ifndef WEBCPP_LEXICAL_DETAIL_UTF8_HPP
#define WEBCPP_LEXICAL_DETAIL_UTF8_HPP

#include <webcpp/lexical/config.hpp>

#include <array>
#include <cstddef>
#include <string_view>

namespace webcpp::lexical::detail {

/**
 Decodes the code point at `s[i]` into `cp` and moves `i` past it; false, `i` and `cp` unchanged,
 for an ill-formed sequence (truncated, overlong, a surrogate, above U+10FFFF, a lone
 continuation byte) or `i` at the end.
*/
constexpr bool decode_utf8(std::string_view s, std::size_t& i, char32_t& cp) noexcept {
    if (i >= s.size()) {
        return false;
    }
    const auto lead = static_cast<unsigned char>(s[i]);
    if (lead < 0x80U) {
        cp = lead;
        ++i;
        return true;
    }
    std::size_t length = 0;
    if (lead >= 0xC2U && lead <= 0xDFU) {
        length = 2;
    } else if (lead >= 0xE0U && lead <= 0xEFU) {
        length = 3;
    } else if (lead >= 0xF0U && lead <= 0xF4U) {
        length = 4;
    } else {
        return false;
    }
    if (s.size() - i < length) {
        return false;
    }
    auto value = static_cast<char32_t>(lead & (0x7FU >> length));
    for (std::size_t k = 1; k < length; ++k) {
        const auto next = static_cast<unsigned char>(s[i + k]);
        if ((next & 0xC0U) != 0x80U) {
            return false;
        }
        value = static_cast<char32_t>((value << 6U) | (next & 0x3FU));
    }
    // The least code point of each length: below it, the sequence is overlong.
    constexpr std::array<char32_t, 5> least{0, 0, 0x80, 0x800, 0x10000};
    if (value < least[length] || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
        return false;
    }
    cp = value;
    i += length;
    return true;
}

}  // namespace webcpp::lexical::detail

#endif  // WEBCPP_LEXICAL_DETAIL_UTF8_HPP
