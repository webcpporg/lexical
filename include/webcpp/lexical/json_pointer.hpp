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
#ifndef WEBCPP_LEXICAL_JSON_POINTER_HPP
#define WEBCPP_LEXICAL_JSON_POINTER_HPP

#include <webcpp/lexical/config.hpp>

#include <webcpp/lexical/detail/utf8.hpp>
#include <webcpp/lexical/grammar.hpp>

#include <boost/url/grammar/digit_chars.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>

#include <cstddef>
#include <optional>
#include <string_view>

namespace webcpp::lexical {

/**
 Whether a relative JSON Pointer may move its array index with a sign and an integer.

 @see "Identifiers", in the guide.
*/
enum class index_manipulation {
    /** An index manipulation is valid, as in draft-bhutton-00, which 2020-12 and v1 cite. */
    allowed,
    /** An index manipulation is refused, as in draft-handrews-01, which 7 and 2019-09 cite. */
    refused,
};

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

/** Where the run of ASCII digits that starts at `s[i]` ends. */
constexpr std::size_t digits_end(std::string_view s, std::size_t i) noexcept {
    while (i < s.size() && urlg::digit_chars(s[i])) {
        ++i;
    }
    return i;
}

/** Past the non-negative integer at `s[i]`, a digit: "0" alone, else every digit of the run. */
constexpr std::size_t integer_end(std::string_view s, std::size_t i) noexcept {
    return s[i] == '0' ? i + 1 : digits_end(s, i + 1);
}

/**
 The length of the relative JSON pointer at the start of `s`: a non-negative integer without a
 leading zero, an index manipulation where `manipulation` allows it, then "#" or a json-pointer;
 nothing when no digit starts `s` or the json-pointer breaks a rule. The rule checks the first
 digit before it calls this, to answer mismatch; this checks it again, as its own guard.
*/
constexpr std::optional<std::size_t> relative_json_pointer_length(
    std::string_view s, index_manipulation manipulation) noexcept {
    if (s.empty() || !urlg::digit_chars(s[0])) {
        return std::nullopt;
    }
    std::size_t i = integer_end(s, 0);
    // A sign without an integer after it is no manipulation: the pointer ends before it.
    if (manipulation == index_manipulation::allowed && i + 1 < s.size() &&
        (s[i] == '+' || s[i] == '-') && urlg::digit_chars(s[i + 1])) {
        i = integer_end(s, i + 1);
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
 text where no `/` starts the input, and never answers mismatch. Once a `/` starts it, inside a
 larger grammar it runs to the end of the input, since a token holds every character the grammar
 could continue with.

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
 The rule of the relative-json-pointer of draft-bhutton-relative-json-pointer-00 section 3 and
 draft-handrews-relative-json-pointer-01 section 3: a non-negative integer, an optional index
 manipulation, then `#` or a json-pointer.

 The integer is `0` or starts with 1 to 9, so `01` is refused. The index manipulation, a sign and
 such an integer (`0+1`, `0-0`), is draft-bhutton-00's, which draft 2020-12 and v1 cite; it may
 come before the `#` too (`1-1#`), as the draft's example `0-1#` does though its ABNF omits it.
 Drafts 7 and 2019-09 cite draft-handrews-01, which has none, so @ref index_manipulation::refused
 refuses it. It stops after the `#`, or where its json-pointer does; where a sign is not a valid
 manipulation, it stops before the sign.

 @see "Identifiers", in the guide.
*/
struct relative_json_pointer_rule_t {
    /** The rule yields the text it matched. */
    using value_type = text;

    /** Whether an index manipulation may follow the integer. */
    index_manipulation manipulation = index_manipulation::allowed;

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

/**
 The rule of the relative JSON pointer, index manipulation allowed;
 `relative_json_pointer_rule_t{index_manipulation::refused}` refuses it.
*/
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

inline result<text> relative_json_pointer_rule_t::parse(char const*& it,
                                                        char const* end) const noexcept {
    if (it == end || !detail::urlg::digit_chars(*it)) {
        return detail::urlg::error::mismatch;
    }
    const std::optional<std::size_t> length = detail::relative_json_pointer_length(
        std::string_view(it, static_cast<std::size_t>(end - it)), manipulation);
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
 @param manipulation Whether an index manipulation (`0+1`) may be valid:
 @ref index_manipulation::allowed for draft 2020-12 and v1,
 @ref index_manipulation::refused for drafts 7 and 2019-09.
 @return `parse(s, grammar::relative_json_pointer_rule_t{manipulation}).has_value()`.
 @see "Identifiers", in the guide.
*/
[[nodiscard]] inline bool is_relative_json_pointer(
    std::string_view s, index_manipulation manipulation = index_manipulation::allowed) noexcept {
    return detail::urlg::parse(s, grammar::relative_json_pointer_rule_t{manipulation}).has_value();
}

}  // namespace webcpp::lexical

#endif  // WEBCPP_LEXICAL_JSON_POINTER_HPP
