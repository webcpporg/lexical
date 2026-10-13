// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// What the syntax of an ECMA-262 pattern reads of Unicode, from the generated tables: whether a
// name is one \p{...} accepts, and whether a code point may start or continue a group's name.

#ifndef WEBCPP_LEXICAL_DETAIL_REGEX_UNICODE_HPP
#define WEBCPP_LEXICAL_DETAIL_REGEX_UNICODE_HPP

#include <webcpp/lexical/config.hpp>

#include <webcpp/lexical/detail/regex_tables.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace webcpp::lexical::detail {

/** Whether the sorted `names` hold `name`. */
constexpr bool has_name(std::span<const std::string_view> names, std::string_view name) noexcept {
    return std::ranges::binary_search(names, name);
}

/**
 Whether `cp` is in one of the sorted `ranges`, each packed as a first code point shifted left by
 21 and a last.
*/
constexpr bool in_ranges(std::span<const std::uint64_t> ranges, char32_t cp) noexcept {
    constexpr unsigned code_point_bits = 21;
    constexpr std::uint64_t mask = (std::uint64_t{1} << code_point_bits) - 1;
    // The first range that starts after cp; the one before it holds cp, if any does.
    const auto after = static_cast<std::size_t>(
        std::ranges::upper_bound(ranges,
                                 (static_cast<std::uint64_t>(cp) << code_point_bits) | mask) -
        ranges.begin());
    return after != 0 && cp <= (ranges[after - 1] & mask);
}

/** Whether `cp` may start a group's name: `$`, `_` or ID_Start. */
constexpr bool is_name_start(char32_t cp) noexcept {
    return cp == '$' || cp == '_' || in_ranges(regex_tables::id_start, cp);
}

/** Whether `cp` may continue a group's name: `$`, ZWNJ, ZWJ or ID_Continue. */
constexpr bool is_name_part(char32_t cp) noexcept {
    return cp == '$' || cp == 0x200C || cp == 0x200D || in_ranges(regex_tables::id_continue, cp);
}

}  // namespace webcpp::lexical::detail

#endif  // WEBCPP_LEXICAL_DETAIL_REGEX_UNICODE_HPP
