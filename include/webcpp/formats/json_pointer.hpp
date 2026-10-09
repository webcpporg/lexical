// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 JSON Pointers, RFC 6901's and draft-handrews-relative-json-pointer-01's relative ones, as
 validators and Boost.URL rules.

 @see "Identifiers", in the guide.
*/
#ifndef WEBCPP_FORMATS_JSON_POINTER_HPP
#define WEBCPP_FORMATS_JSON_POINTER_HPP

#include <webcpp/formats/config.hpp>

#include <webcpp/formats/detail/utf8.hpp>
#include <webcpp/formats/grammar.hpp>

#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>

#include <cstddef>
#include <optional>
#include <string_view>

namespace webcpp::formats {

namespace detail {

namespace urlg = boost::urls::grammar;

/**
 The length of the RFC 6901 json-pointer at the start of `s`: "/"-led reference tokens, each "~"
 followed by "0" or "1", every code point well-formed UTF-8; nothing when a token breaks a rule.
*/
constexpr std::optional<std::size_t> json_pointer_length(std::string_view s) noexcept {
    std::size_t i = 0;
    while (i < s.size() && s[i] == '/') {
        ++i;
        while (i < s.size() && s[i] != '/') {
            if (s[i] == '~') {
                if (i + 1 >= s.size() || (s[i + 1] != '0' && s[i + 1] != '1')) {
                    return std::nullopt;
                }
                i += 2;
                continue;
            }
            char32_t cp = 0;
            if (!decode_utf8(s, i, cp)) {
                return std::nullopt;
            }
        }
    }
    return i;
}

/**
 The length of the relative JSON pointer at the start of `s`: a non-negative integer without a
 leading zero, then "#" or a json-pointer; nothing when no digit starts `s` or the pointer breaks
 a rule.
*/
constexpr std::optional<std::size_t> relative_json_pointer_length(std::string_view s) noexcept {
    if (s.empty() || s[0] < '0' || s[0] > '9') {
        return std::nullopt;
    }
    std::size_t i = 1;
    if (s[0] != '0') {
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            ++i;
        }
    }
    if (i < s.size() && s[i] == '#') {
        return i + 1;
    }
    std::string_view rest = s;
    rest.remove_prefix(i);
    const std::optional<std::size_t> pointer = json_pointer_length(rest);
    if (!pointer) {
        return std::nullopt;
    }
    return i + *pointer;
}

}  // namespace detail

namespace grammar {

/**
 The rule of RFC 6901 section 3's json-pointer: zero or more reference tokens, each led by `/`.

 A token holds any code point but `/` and `~`, NUL included, and `~0` or `~1`; the input is
 decoded as UTF-8 and must be well-formed. The empty pointer is valid, so the rule matches empty
 text where no `/` starts the input, and never answers mismatch. Inside a larger grammar it runs
 to the end of the input, since a token holds every character the grammar could continue with.

 @see "Identifiers", in the guide.
*/
struct json_pointer_rule_t {
    /** The rule yields the text it matched. */
    using value_type = text;

    /**
     Matches a json-pointer at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The text matched, empty when no `/` starts the input; `error::invalid` when a token
     holds a `~` that is not `~0` or `~1`, or ill-formed UTF-8.
    */
    [[nodiscard]] result<text> parse(char const*& it, char const* end) const noexcept;
};

/**
 The rule of draft-handrews-relative-json-pointer-01 section 3's relative-json-pointer: a
 non-negative integer, then `#` or a json-pointer.

 The integer is `0` or starts with 1 to 9, so `01` is refused; no index manipulation (`0+1`), as
 drafts 2019-09 to v1 cite this draft. It stops after the `#`, or where its json-pointer does.

 @see "Identifiers", in the guide.
*/
struct relative_json_pointer_rule_t {
    /** The rule yields the text it matched. */
    using value_type = text;

    /**
     Matches a relative-json-pointer at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The text matched; `error::mismatch` when no digit starts it; `error::invalid` when
     its json-pointer breaks a rule.
    */
    [[nodiscard]] result<text> parse(char const*& it, char const* end) const noexcept;
};

/** The rule of RFC 6901's json-pointer. */
inline constexpr json_pointer_rule_t json_pointer_rule{};

/** The rule of the relative JSON pointer. */
inline constexpr relative_json_pointer_rule_t relative_json_pointer_rule{};

// parse reads no member, yet stays a const member: Boost.URL's Rule concept calls it on the rule.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline result<text> json_pointer_rule_t::parse(char const*& it, char const* end) const noexcept {
    const std::optional<std::size_t> length =
        detail::json_pointer_length(std::string_view(it, static_cast<std::size_t>(end - it)));
    if (!length) {
        return detail::urlg::error::invalid;
    }
    char const* const first = it;
    it += *length;
    return text(first, *length);
}

// parse reads no member, yet stays a const member: Boost.URL's Rule concept calls it on the rule.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline result<text> relative_json_pointer_rule_t::parse(char const*& it,
                                                        char const* end) const noexcept {
    if (it == end || *it < '0' || *it > '9') {
        return detail::urlg::error::mismatch;
    }
    const std::optional<std::size_t> length = detail::relative_json_pointer_length(
        std::string_view(it, static_cast<std::size_t>(end - it)));
    if (!length) {
        return detail::urlg::error::invalid;
    }
    char const* const first = it;
    it += *length;
    return text(first, *length);
}

}  // namespace grammar

/**
 Whether `s` is an RFC 6901 JSON Pointer, JSON Schema's `json-pointer`.

 @param s Any byte string.
 @return `parse(s, grammar::json_pointer_rule).has_value()`.
 @see "Identifiers", in the guide.
*/
[[nodiscard]] inline bool is_json_pointer(std::string_view s) noexcept {
    return detail::urlg::parse(s, grammar::json_pointer_rule).has_value();
}

/**
 Whether `s` is a relative JSON Pointer, JSON Schema's `relative-json-pointer`.

 @param s Any byte string.
 @return `parse(s, grammar::relative_json_pointer_rule).has_value()`.
 @see "Identifiers", in the guide.
*/
[[nodiscard]] inline bool is_relative_json_pointer(std::string_view s) noexcept {
    return detail::urlg::parse(s, grammar::relative_json_pointer_rule).has_value();
}

}  // namespace webcpp::formats

#endif  // WEBCPP_FORMATS_JSON_POINTER_HPP
