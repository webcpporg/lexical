// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Regular expressions, JSON Schema's `regex`: whether a string is an ECMA-262 pattern in `u` mode,
 judged by lexical's own syntax validator. Nothing is compiled or run, and nothing is allocated.

 @see "Regular expressions", in the guide.
*/
#ifndef WEBCPP_LEXICAL_REGEX_HPP
#define WEBCPP_LEXICAL_REGEX_HPP

#include <webcpp/lexical/config.hpp>

#include <webcpp/lexical/detail/regex_validator.hpp>

#include <cstddef>
#include <string_view>

namespace webcpp::lexical {

/**
 The longest pattern `is_regex` judges, in bytes: 65,536.

 A longer pattern is answered `false` without being read, which bounds the work that a hostile
 pattern can make the validator do.

 @see "Regular expressions", in the guide.
*/
inline constexpr std::size_t max_regex_length = 65536;

/**
 The deepest a pattern's groups may nest for `is_regex` to judge it: 32 levels of groups,
 lookarounds and modifiers, one inside the other.

 A pattern nested deeper is answered `false` without being judged. The validator reads a group by
 calling itself, and this bound limits that recursion, so that a hostile pattern cannot exhaust the
 stack of a WebAssembly program.

 @see "Regular expressions", in the guide.
*/
inline constexpr std::size_t max_regex_nesting = 32;

/**
 Whether `pattern` is a regular expression, JSON Schema's `regex`: an ECMA-262 `Pattern` in `u`
 mode, with ECMAScript 2025's modifiers and duplicate group names.

 Only the syntax and the early errors are checked: the pattern is never compiled or run. A pattern
 that is not well-formed UTF-8 is invalid, and a NUL is a literal like any other code point. A
 pattern longer than `max_regex_length`, or nested deeper than `max_regex_nesting`, is answered
 `false` without being judged. Nothing is allocated, and nothing throws.

 @param pattern Any byte string.
 @return Whether the pattern is an ECMA-262 regular expression in `u` mode.
 @see "Regular expressions", in the guide.
*/
[[nodiscard]] inline bool is_regex(std::string_view pattern) noexcept {
    if (pattern.size() > max_regex_length) {
        return false;
    }
    return detail::check_regex(pattern, max_regex_nesting).verdict == detail::regex_verdict::valid;
}

}  // namespace webcpp::lexical

#endif  // WEBCPP_LEXICAL_REGEX_HPP
