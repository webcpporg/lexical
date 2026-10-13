// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 E-mail addresses, RFC 5321's Mailbox and RFC 6531's internationalized one, as validators and
 Boost.URL rules composed of Boost.URL's combinators over lexical's terminals and Boost.URL's IP
 rules.

 @see "Addresses", in the guide.
*/
#ifndef WEBCPP_LEXICAL_EMAIL_HPP
#define WEBCPP_LEXICAL_EMAIL_HPP

#include <webcpp/lexical/config.hpp>

#include <webcpp/lexical/detail/charsets.hpp>
#include <webcpp/lexical/detail/rules.hpp>
#include <webcpp/lexical/detail/utf8.hpp>
#include <webcpp/lexical/grammar.hpp>
#include <webcpp/lexical/hostname.hpp>

#include <boost/url/grammar/delim_rule.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/tuple_rule.hpp>
#include <boost/url/grammar/variant_rule.hpp>
#include <boost/url/rfc/ipv4_address_rule.hpp>
#include <boost/url/rfc/ipv6_address_rule.hpp>

#include <cstddef>
#include <optional>
#include <string_view>

namespace webcpp::lexical {

namespace detail {

namespace urlg = boost::urls::grammar;

/**
 The longest Local-part RFC 5321 4.5.3.1.1 allows, in octets, a Quoted-string's quotes included.
*/
inline constexpr std::size_t max_local_part_length = 64;

/**
 The longest Mailbox, in octets: RFC 5321 4.5.3.1.3's path of 256 octets less its two angle
 brackets.
*/
inline constexpr std::size_t max_mailbox_length = 254;

/**
 The length of the Atom at the start of `s`: 1*atext, a UTF8-non-ascii sequence counting as atext
 when `utf8`.
*/
constexpr std::size_t atom_length(std::string_view s, bool utf8) noexcept {
    std::size_t i = 0;
    while (i < s.size()) {
        if (static_cast<unsigned char>(s[i]) < 0x80U) {
            if (!atext_chars(s[i])) {
                break;
            }
            ++i;
            continue;
        }
        std::size_t next = i;
        char32_t cp = 0;
        if (!utf8 || !decode_utf8(s, next, cp)) {
            break;
        }
        i = next;
    }
    return i;
}

/**
 The length of the Dot-string at the start of `s`, Atom *("." Atom); a dot no Atom follows is not
 its.
*/
constexpr std::size_t dot_string_length(std::string_view s, bool utf8) noexcept {
    std::size_t length = atom_length(s, utf8);
    if (length == 0) {
        return 0;
    }
    while (length < s.size() && s[length] == '.') {
        std::string_view after = s;
        after.remove_prefix(length + 1);
        const std::size_t atom = atom_length(after, utf8);
        if (atom == 0) {
            break;
        }
        length += 1 + atom;
    }
    return length;
}

/**
 The length of the Quoted-string at the start of `s`, quotes included; nothing when `s` does not
 start with a double quote, or when the string breaks or never closes.
*/
constexpr std::optional<std::size_t> quoted_string_length(std::string_view s, bool utf8) noexcept {
    if (s.empty() || s.front() != '"') {
        return std::nullopt;
    }
    std::size_t i = 1;
    while (i < s.size()) {
        const char c = s[i];
        if (c == '"') {
            return i + 1;
        }
        if (c == '\\') {
            if (i + 1 >= s.size() || !quoted_pair_chars(s[i + 1])) {
                return std::nullopt;
            }
            i += 2;
            continue;
        }
        if (static_cast<unsigned char>(c) < 0x80U) {
            if (!qtext_smtp_chars(c)) {
                return std::nullopt;
            }
            ++i;
            continue;
        }
        char32_t cp = 0;
        if (!utf8 || !decode_utf8(s, i, cp)) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

/**
 RFC 5321 4.1.2's Local-part, or RFC 6531 3.3's when `utf8`: a Dot-string or a Quoted-string of at
 most 64 octets.
*/
struct local_part_rule_t {
    /** The rule yields the local part it matched. */
    using value_type = grammar::text;

    /** Whether UTF8-non-ascii is atext and qtext too (RFC 6531 3.3). */
    bool utf8 = false;

    /**
     Matches a local part at `it`: mismatch when none starts there, invalid when one breaks or is
     too long.
    */
    grammar::result<grammar::text> parse(char const*& it, char const* end) const noexcept {
        const std::string_view rest(it, static_cast<std::size_t>(end - it));
        std::size_t length = 0;
        if (!rest.empty() && rest.front() == '"') {
            const std::optional<std::size_t> quoted = quoted_string_length(rest, utf8);
            if (!quoted) {
                return urlg::error::invalid;
            }
            length = *quoted;
        } else {
            length = dot_string_length(rest, utf8);
            if (length == 0) {
                return urlg::error::mismatch;
            }
        }
        if (length > max_local_part_length) {
            return urlg::error::invalid;
        }
        char const* const first = it;
        it += length;
        return grammar::text(first, length);
    }
};

/** RFC 5321's Local-part. */
inline constexpr local_part_rule_t local_part_rule{};

/** RFC 6531 3.3's Local-part: atext and qtextSMTP with UTF8-non-ascii; quoted-pairSMTP stays ASCII.
 */
inline constexpr local_part_rule_t idn_local_part_rule{true};

/**
 RFC 5321 4.1.3's address-literal: "[" IPv4-address-literal "]" or "[IPv6:" IPv6-addr "]", the tag
 in any case.
*/
inline constexpr auto address_literal_rule = urlg::tuple_rule(
    urlg::squelch(urlg::delim_rule('[')),
    urlg::variant_rule(
        boost::urls::ipv4_address_rule,
        urlg::tuple_rule(urlg::squelch(ci_literal_rule("IPv6:")), boost::urls::ipv6_address_rule)),
    urlg::squelch(urlg::delim_rule(']')));

/**
 Matches a Mailbox at `it`, `local` its local part and `host` its domain: the local part, the at
 sign, then an address literal where "[" follows, else `host`, whose error is returned as it is;
 invalid when the whole is longer than 254 octets.

 Tip: the domain is dispatched here rather than made an alternative of a variant_rule, which
 answers mismatch when every alternative fails and would lose idna's reason.
*/
template <class Host>
grammar::result<grammar::text> parse_mailbox(char const*& it, char const* end,
                                             const local_part_rule_t& local,
                                             const Host& host) noexcept {
    char const* const first = it;
    if (const auto part = local.parse(it, end); !part) {
        return part.error();
    }
    if (it == end || *it != '@') {
        it = first;
        return urlg::error::invalid;
    }
    ++it;
    if (it != end && *it == '[') {
        if (const auto literal = urlg::parse(it, end, address_literal_rule); !literal) {
            it = first;
            return urlg::error::invalid;
        }
    } else if (const auto domain = host.parse(it, end); !domain) {
        it = first;
        if (domain.error() == urlg::error::mismatch) {
            return urlg::error::invalid;
        }
        return domain.error();
    }
    const auto length = static_cast<std::size_t>(it - first);
    if (length > max_mailbox_length) {
        it = first;
        return urlg::error::invalid;
    }
    return grammar::text(first, length);
}

}  // namespace detail

namespace grammar {

/**
 The rule of RFC 5321 4.1.2's Mailbox: a local part, the at sign, and a domain or an address
 literal.

 The local part is a Dot-string of atoms (RFC 5322's `atext`) or a Quoted-string, at most 64
 octets. The domain is @ref hostname_rule, A-labels included, whose error, idna's for an A-label,
 the rule returns as it is; an address literal is `[` and an IPv4 address `]`, or `[IPv6:` and an
 IPv6 address `]`, the tag in any case: RFC 5321's General-address-literal needs a tag registered
 with IANA, and `IPv6` is the only one. The whole Mailbox is at most 254 octets, RFC 5321
 4.5.3.1.3's path of 256 less its angle brackets. It stops where the domain or the literal does.

 @see "Addresses", in the guide.
*/
struct email_rule_t {
    /** The rule yields the text it matched. */
    using value_type = text;

    /**
     Matches a Mailbox at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The text matched; `error::mismatch` when no local part starts at `it`;
     `error::invalid` when the rest breaks the grammar or the Mailbox is longer than 254 octets;
     idna's error when idna refuses an A-label of the domain.
    */
    [[nodiscard]] result<text> parse(char const*& it, char const* end) const noexcept;
};

/** The rule of RFC 5321's Mailbox. */
inline constexpr email_rule_t email_rule{};

// A const member, as Boost.URL's Rule concept declares parse, though it reads no member.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline result<text> email_rule_t::parse(char const*& it, char const* end) const noexcept {
    return detail::parse_mailbox(it, end, detail::local_part_rule, hostname_rule);
}

/**
 The rule of RFC 6531 3.3's internationalized Mailbox: @ref email_rule with UTF-8 in the local part
 and an internationalized domain.

 `atext` and `qtextSMTP` also hold any well-formed non-ASCII UTF-8 sequence (RFC 6532 3.1, C1
 controls and noncharacters included), `quoted-pairSMTP` stays ASCII, the local part is at most 64
 octets and is not normalized; the domain is @ref idn_hostname_rule, whose error, idna's, the rule
 returns as it is, or an address literal. The whole Mailbox is at most 254 octets as written, in
 UTF-8, the form an SMTPUTF8 envelope carries, whatever its domain's ASCII form measures.

 @see "Addresses", in the guide.
*/
struct idn_email_rule_t {
    /** The rule yields the text it matched. */
    using value_type = text;

    /**
     Matches an internationalized Mailbox at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The text matched; `error::mismatch` when no local part starts at `it`;
     `error::invalid` when the rest breaks the grammar or the Mailbox is longer than 254 octets;
     idna's error when idna refuses the domain.
    */
    [[nodiscard]] result<text> parse(char const*& it, char const* end) const noexcept;
};

/** The rule of RFC 6531's Mailbox. */
inline constexpr idn_email_rule_t idn_email_rule{};

// A const member, as Boost.URL's Rule concept declares parse, though it reads no member.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline result<text> idn_email_rule_t::parse(char const*& it, char const* end) const noexcept {
    return detail::parse_mailbox(it, end, detail::idn_local_part_rule, idn_hostname_rule);
}

}  // namespace grammar

/**
 Whether `s` is an RFC 5321 Mailbox, JSON Schema's `email`.

 @param s Any byte string.
 @return `parse(s, grammar::email_rule).has_value()`.
 @see "Addresses", in the guide.
*/
[[nodiscard]] inline bool is_email(std::string_view s) noexcept {
    return detail::urlg::parse(s, grammar::email_rule).has_value();
}

/**
 Whether `s` is an RFC 6531 internationalized Mailbox, JSON Schema's `idn-email`.

 @param s Any byte string.
 @return `parse(s, grammar::idn_email_rule).has_value()`.
 @see "Addresses", in the guide.
*/
[[nodiscard]] inline bool is_idn_email(std::string_view s) noexcept {
    return detail::urlg::parse(s, grammar::idn_email_rule).has_value();
}

}  // namespace webcpp::lexical

#endif  // WEBCPP_LEXICAL_EMAIL_HPP
