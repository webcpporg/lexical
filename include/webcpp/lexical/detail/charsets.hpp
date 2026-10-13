// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The ASCII character sets of lexical's grammars, as Boost.URL's lut_chars, and RFC 3987's code
// point classes.

#ifndef WEBCPP_LEXICAL_DETAIL_CHARSETS_HPP
#define WEBCPP_LEXICAL_DETAIL_CHARSETS_HPP

#include <webcpp/lexical/config.hpp>

#include <boost/url/grammar/lut_chars.hpp>

namespace webcpp::lexical::detail {

namespace urlg = boost::urls::grammar;

/** Letters, digits and the hyphen: the characters of a host name's label (RFC 1123 2.1). */
inline constexpr urlg::lut_chars ldh_chars(
    "-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

/** A host name's characters: its labels' and the dot. */
inline constexpr urlg::lut_chars hostname_chars(
    "-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

/** RFC 5322 3.2.3's atext: letters, digits and the symbols below. */
inline constexpr urlg::lut_chars atext_chars(
    "!#$%&'*+-/=?^_`{|}~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

/**
 RFC 5321 4.1.2's qtextSMTP: the space and printable ASCII, but the double quote and the
 backslash.
*/
inline constexpr urlg::lut_chars qtext_smtp_chars([](char c) noexcept {
    return c >= ' ' && c <= '~' && c != '"' && c != '\\';
});

/** What may follow the backslash of RFC 5321 4.1.2's quoted-pairSMTP: %d32-126. */
inline constexpr urlg::lut_chars quoted_pair_chars([](char c) noexcept {
    return c >= ' ' && c <= '~';
});

/**
 RFC 6570 2.1's ASCII literals with erratum 6937: %x21 / %x23-24 / %x26-3B / %x3D / %x3F-5B /
 %x5D / %x5F / %x61-7A / %x7E.
*/
inline constexpr urlg::lut_chars template_literal_chars([](char c) noexcept {
    return c == 0x21 || (c >= 0x23 && c <= 0x24) || (c >= 0x26 && c <= 0x3B) || c == 0x3D ||
           (c >= 0x3F && c <= 0x5B) || c == 0x5D || c == 0x5F || (c >= 0x61 && c <= 0x7A) ||
           c == 0x7E;
});

/** RFC 6570 2.3's varchar but its pct-encoded triplets: letters, digits and the underscore. */
inline constexpr urlg::lut_chars varchar_chars(
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz");

/** RFC 6570 2.2's operators, op-reserve's included as the ABNF admits them. */
inline constexpr urlg::lut_chars template_operator_chars("+#./;?&=,!@|");

/** Whether `cp` is in RFC 3987 2.2's ucschar. */
constexpr bool is_ucschar(char32_t cp) noexcept {
    if ((cp >= 0xA0 && cp <= 0xD7FF) || (cp >= 0xF900 && cp <= 0xFDCF) ||
        (cp >= 0xFDF0 && cp <= 0xFFEF)) {
        return true;
    }
    // Plane 14 starts at U+E1000; every other supplementary plane up to 14 holds all but its last
    // two code points.
    if (cp >= 0xE0000 && cp <= 0xE0FFF) {
        return false;
    }
    return cp >= 0x10000 && cp <= 0xEFFFD && (cp & 0xFFFFU) <= 0xFFFDU;
}

/** Whether `cp` is in RFC 3987 2.2's iprivate. */
constexpr bool is_iprivate(char32_t cp) noexcept {
    return (cp >= 0xE000 && cp <= 0xF8FF) || (cp >= 0xF0000 && cp <= 0xFFFFD) ||
           (cp >= 0x100000 && cp <= 0x10FFFD);
}

/** Whether `cp` is a bidirectional formatting character, which RFC 3987 4.1 refuses in an IRI. */
constexpr bool is_bidi_format(char32_t cp) noexcept {
    return cp == 0x200E || cp == 0x200F || (cp >= 0x202A && cp <= 0x202E);
}

}  // namespace webcpp::lexical::detail

#endif  // WEBCPP_LEXICAL_DETAIL_CHARSETS_HPP
