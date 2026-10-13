// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Regular expressions: whether a string is an ECMA-262 pattern in `u` mode, read by formats'
 translator and compiled by Boost.Regex. The one header of formats that includes Boost.Regex, so
 `<webcpp/formats.hpp>` leaves it out.

 @see "Regular expressions", in the guide.
*/
#ifndef WEBCPP_FORMATS_REGEX_HPP
#define WEBCPP_FORMATS_REGEX_HPP

#include <webcpp/formats/config.hpp>

#include <webcpp/formats/detail/regex_traits.hpp>
#include <webcpp/formats/detail/regex_translator.hpp>

#include <boost/regex.hpp>

#include <cstddef>
#include <memory_resource>
#include <string>
#include <string_view>

namespace webcpp::formats {

/**
 The longest pattern `is_regex` judges, in bytes: 65,536.

 A longer pattern is answered `false` without being read, so that a hostile one cannot make
 Boost.Regex's compilation take more memory than a WebAssembly program starts with: the
 translation and Boost.Regex's program take some 20 bytes for each code point of a pattern.

 @see "Regular expressions", in the guide.
*/
inline constexpr std::size_t max_regex_length = 65536;

/**
 The deepest a pattern's groups may nest for `is_regex` to judge it: 32 levels of groups,
 lookarounds and modifiers, one inside the other.

 A pattern nested deeper is answered `false` without being judged, so that neither formats'
 translator nor Boost.Regex's parser, which read a group by calling themselves, can overflow the
 64 KiB stack of a WebAssembly program on a hostile pattern.

 @see "Regular expressions", in the guide.
*/
inline constexpr std::size_t max_regex_nesting = 32;

/**
 The most alternatives a pattern's translation may hold for `is_regex` to judge it: 4,096, each
 `|`, quantifier and backreference counted.

 A pattern with more is answered `false` without being judged: Boost.Regex keeps each with a map
 of 256 bytes, and a hostile pattern of `|` would make its compilation take some 150 bytes for each
 of its own.

 @see "Regular expressions", in the guide.
*/
inline constexpr std::size_t max_regex_alternatives = 4096;

namespace detail {

/** Boost.Regex over code points, with ECMA-262's classes. */
using ecma_regex = boost::basic_regex<char32_t, regex_traits>;

/** Whether Boost.Regex compiles `pattern`, a translation: its error is a value, never thrown. */
inline bool boost_compiles(std::u32string_view pattern) {
    namespace constants = boost::regex_constants;
    // Boost.Regex's options are int constants, joined with | as its documentation joins them.
    // NOLINTBEGIN(bugprone-signed-bitwise)
    constexpr auto options =
        constants::perl | constants::no_except | constants::no_mod_m | constants::no_mod_s;
    // NOLINTEND(bugprone-signed-bitwise)
    ecma_regex compiled;
    compiled.assign(pattern.data(), pattern.data() + pattern.size(), options);
    return compiled.status() == 0;
}

/**
 The translation with each lookbehind written as a lookahead, `(?<=` as `(?=(?:)` and `(?<!` as
 `(?!(?:)`: what Boost.Regex compiles whatever the lookbehinds' widths. The empty group keeps an
 empty lookbehind's lookahead from being empty, which Boost refuses. No other `(?<` is in a
 translation, which writes a literal `(`, `?` or `<` as `\\x{...}` and a group's name as its number.
*/
inline std::pmr::u32string lookbehinds_as_lookaheads(const std::pmr::u32string& pattern) {
    std::pmr::u32string ahead(pattern.get_allocator());
    ahead.reserve(pattern.size());
    for (std::size_t k = 0; k < pattern.size(); ++k) {
        const bool opens_lookbehind = pattern.compare(k, 3, U"(?<") == 0 &&
                                      k + 3 < pattern.size() &&
                                      (pattern[k + 3] == '=' || pattern[k + 3] == '!');
        if (opens_lookbehind) {
            ahead += U"(?";
            ahead.push_back(pattern[k + 3]);
            ahead += U"(?:)";
            k += 3;
            continue;
        }
        ahead.push_back(pattern[k]);
    }
    return ahead;
}

}  // namespace detail

/**
 Whether `pattern` is a regular expression, JSON Schema's `regex`: ECMA-262's `Pattern` in `u`
 mode, with ECMAScript 2025's modifiers and duplicate group names.

 formats' translator reads the pattern and refuses what ECMA-262 refuses in `u` mode, then
 Boost.Regex compiles its translation, over code points, with Unicode 17's properties. Boost.Regex
 compiles every translation but one whose lookbehind's width varies, which ECMA-262 allows and
 Boost.Regex does not: such a pattern is valid when Boost.Regex compiles it with its lookbehinds
 written as lookaheads. A pattern that is not well-formed UTF-8 is invalid, and a NUL is a literal
 like any other code point. A pattern longer than `max_regex_length`, nested deeper than
 `max_regex_nesting` or whose translation holds more than `max_regex_alternatives` alternatives
 is answered false without being judged. No error is thrown: Boost.Regex reports one as a status.

 Every allocation of the translator goes through `resource`, and is given back before the function
 returns; Boost.Regex's compilation allocates outside any memory resource, through the global
 `operator new`, 5 to 11 times for a short pattern, and gives it back too. This header alone does
 not keep formats' rule that a validator allocates only through a resource it is given.

 @param pattern Any byte string.
 @param resource Where the translator takes its memory.
 @return Whether the pattern is an ECMA-262 regular expression in `u` mode.
 @throws std::bad_alloc When `resource` or the global `operator new` cannot allocate; formats
 raises nothing of its own.
 @see "Regular expressions", in the guide.
*/
[[nodiscard]] inline bool is_regex(std::string_view pattern, std::pmr::memory_resource& resource) {
    if (pattern.size() > max_regex_length) {
        return false;
    }
    const detail::regex_translation translation = detail::translate_regex(
        pattern, {.nesting = max_regex_nesting, .alternatives = max_regex_alternatives}, resource);
    if (translation.verdict != detail::regex_verdict::valid) {
        return false;
    }
    if (detail::boost_compiles(translation.pattern)) {
        return true;
    }
    return translation.has_lookbehind &&
           detail::boost_compiles(detail::lookbehinds_as_lookaheads(translation.pattern));
}

/**
 Whether `pattern` is a regular expression, JSON Schema's `regex`, the translator's memory from
 `std::pmr::get_default_resource()`.

 @param pattern Any byte string.
 @return Whether the pattern is an ECMA-262 regular expression in `u` mode.
 @throws std::bad_alloc When the default resource or the global `operator new` cannot allocate.
 @see "Regular expressions", in the guide.
*/
[[nodiscard]] inline bool is_regex(std::string_view pattern) {
    return is_regex(pattern, *std::pmr::get_default_resource());
}

}  // namespace webcpp::formats

#endif  // WEBCPP_FORMATS_REGEX_HPP
