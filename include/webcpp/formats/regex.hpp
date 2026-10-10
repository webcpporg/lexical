// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Regular expressions: whether a string is an ECMA-262 pattern in `u` mode, through webcpp::regex.
 The one header of formats that includes it, so `<webcpp/formats.hpp>` leaves it out.

 @see "Regular expressions", in the guide.
*/
#ifndef WEBCPP_FORMATS_REGEX_HPP
#define WEBCPP_FORMATS_REGEX_HPP

#include <webcpp/formats/config.hpp>

#include <webcpp/regex.hpp>

#include <memory_resource>
#include <string_view>

namespace webcpp::formats {

/**
 Whether `pattern` is a regular expression of the configured webcpp::regex backend, JSON Schema's
 `regex`: ECMA-262 in `u` mode on SRELL, the default, and Boost.Regex.

 webcpp::regex refuses, before its engine compiles the rest, a pattern that is not well-formed
 UTF-8 and SRELL's extensions, a backslash before A, Z or z. It also refuses, without judging it,
 a pattern past one of its bounds, nested deeper than `webcpp::regex::max_nesting` or with more
 alternatives than `webcpp::regex::max_alternatives`: ECMA-262 may allow it, but no search of
 this program could use it, so the answer is false. Every allocation of the compilation goes
 through `resource` where `webcpp::regex::honours<>::memory` holds, which it does on SRELL.

 @param pattern Any byte string.
 @param resource Where the compilation takes its memory, all of it given back before the function
 returns.
 @return Whether the pattern compiles: false for a pattern ECMA-262 refuses, and for one past a
 bound of webcpp::regex.
 @throws std::bad_alloc When `resource` cannot allocate, or what it throws instead; formats raises
 nothing of its own.
 @see "Regular expressions", in the guide.
*/
[[nodiscard]] inline bool is_regex(std::string_view pattern, std::pmr::memory_resource& resource) {
    namespace re = webcpp::regex;
    return re::regex::compile(pattern,
                              {.syntax = re::regex_constants::ECMAScript, .memory = &resource})
        .has_value();
}

/**
 Whether `pattern` is a regular expression of the configured webcpp::regex backend, its
 compilation's memory from `std::pmr::get_default_resource()`.

 @param pattern Any byte string.
 @return Whether the pattern compiles: false for a pattern ECMA-262 refuses, and for one past a
 bound of webcpp::regex.
 @throws std::bad_alloc When the default resource cannot allocate.
 @see "Regular expressions", in the guide.
*/
[[nodiscard]] inline bool is_regex(std::string_view pattern) {
    return is_regex(pattern, *std::pmr::get_default_resource());
}

}  // namespace webcpp::formats

#endif  // WEBCPP_FORMATS_REGEX_HPP
