// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 UUIDs, RFC 9562 section 4's string form, as a validator and a Boost.URL rule.

 @see "Identifiers", in the guide.
*/
#ifndef WEBCPP_LEXICAL_UUID_HPP
#define WEBCPP_LEXICAL_UUID_HPP

#include <webcpp/lexical/config.hpp>

#include <webcpp/lexical/detail/rules.hpp>
#include <webcpp/lexical/grammar.hpp>

#include <boost/url/grammar/delim_rule.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/hexdig_chars.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/tuple_rule.hpp>

#include <cstddef>
#include <string_view>

namespace webcpp::lexical {

namespace detail {

namespace urlg = boost::urls::grammar;

/** RFC 9562 4's shape: 8HEXDIG "-" 4HEXDIG "-" 4HEXDIG "-" 4HEXDIG "-" 12HEXDIG, any case. */
inline constexpr auto uuid_shape =
    urlg::tuple_rule(exactly<8>(urlg::hexdig_chars), urlg::squelch(urlg::delim_rule('-')),
                     exactly<4>(urlg::hexdig_chars), urlg::squelch(urlg::delim_rule('-')),
                     exactly<4>(urlg::hexdig_chars), urlg::squelch(urlg::delim_rule('-')),
                     exactly<4>(urlg::hexdig_chars), urlg::squelch(urlg::delim_rule('-')),
                     exactly<12>(urlg::hexdig_chars));

}  // namespace detail

namespace grammar {

/**
 The rule of RFC 9562 section 4's UUID: 32 hexadecimal digits in groups of 8, 4, 4, 4 and 12,
 joined by hyphens.

 Either case; the version and variant nibbles are not checked; no braces and no `urn:uuid:`.
 It stops after the twelfth digit of the last group.

 @see "Identifiers", in the guide.
*/
struct uuid_rule_t {
    /** The rule yields the text it matched. */
    using value_type = text;

    /**
     Matches a UUID at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The text matched; `error::mismatch` when no hexadecimal digit starts it;
     `error::invalid` when it started and broke the shape.
    */
    [[nodiscard]] result<text> parse(char const*& it, char const* end) const noexcept;
};

/** The rule of RFC 9562's UUID. */
inline constexpr uuid_rule_t uuid_rule{};

// parse reads no member, yet stays a const member: Boost.URL's Rule concept calls it on the rule.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline result<text> uuid_rule_t::parse(char const*& it, char const* end) const noexcept {
    if (it == end || !detail::urlg::hexdig_chars(*it)) {
        return detail::urlg::error::mismatch;
    }
    char const* const first = it;
    if (const auto uuid = detail::urlg::parse(it, end, detail::uuid_shape); !uuid) {
        it = first;
        return detail::urlg::error::invalid;
    }
    return text(first, static_cast<std::size_t>(it - first));
}

}  // namespace grammar

/**
 Whether `s` is a UUID in RFC 9562's string form, JSON Schema's `uuid`.

 @param s Any byte string.
 @return `parse(s, grammar::uuid_rule).has_value()`.
 @see "Identifiers", in the guide.
*/
[[nodiscard]] inline bool is_uuid(std::string_view s) noexcept {
    return detail::urlg::parse(s, grammar::uuid_rule).has_value();
}

}  // namespace webcpp::lexical

#endif  // WEBCPP_LEXICAL_UUID_HPP
