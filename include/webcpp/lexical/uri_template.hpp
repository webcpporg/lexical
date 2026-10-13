// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 URI Templates, RFC 6570 section 2 with erratum 6937, as a validator and a Boost.URL rule.

 @see "URI templates", in the guide.
*/
#ifndef WEBCPP_LEXICAL_URI_TEMPLATE_HPP
#define WEBCPP_LEXICAL_URI_TEMPLATE_HPP

#include <webcpp/lexical/config.hpp>

#include <webcpp/lexical/detail/charsets.hpp>
#include <webcpp/lexical/detail/rules.hpp>
#include <webcpp/lexical/detail/utf8.hpp>
#include <webcpp/lexical/grammar.hpp>

#include <boost/url/grammar/delim_rule.hpp>
#include <boost/url/grammar/digit_chars.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/hexdig_chars.hpp>
#include <boost/url/grammar/lut_chars.hpp>
#include <boost/url/grammar/not_empty_rule.hpp>
#include <boost/url/grammar/optional_rule.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/range_rule.hpp>
#include <boost/url/grammar/tuple_rule.hpp>
#include <boost/url/grammar/variant_rule.hpp>
#include <boost/url/rfc/pct_encoded_rule.hpp>

#include <cstddef>
#include <string_view>

namespace webcpp::lexical {

namespace detail {

namespace urlg = boost::urls::grammar;

/** The length of the pct-encoded triplet at `s[i]`: 3, or 0 when there is none. */
constexpr std::size_t pct_encoded_length(std::string_view s, std::size_t i) noexcept {
    return i < s.size() && s[i] == '%' && s.size() - i >= 3 && urlg::hexdig_chars(s[i + 1]) &&
                   urlg::hexdig_chars(s[i + 2])
               ? 3
               : 0;
}

/**
 RFC 6570's literals: one or more literal characters, pct-encoded triplets, ucschar or iprivate
 code points.
*/
struct template_literals_rule_t {
    /** The rule yields the literals it matched. */
    using value_type = grammar::text;

    /** Matches the literals at `it`, or leaves `it` and answers mismatch when none is there. */
    grammar::result<grammar::text> parse(char const*& it, char const* end) const noexcept;
};

// parse reads no member, yet stays a const member: Boost.URL's Rule concept calls it on the rule.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline grammar::result<grammar::text> template_literals_rule_t::parse(
    char const*& it, char const* end) const noexcept {
    const std::string_view rest(it, static_cast<std::size_t>(end - it));
    std::size_t i = 0;
    while (i < rest.size()) {
        if (const std::size_t pct = pct_encoded_length(rest, i); pct != 0) {
            i += pct;
            continue;
        }
        if (static_cast<unsigned char>(rest[i]) < 0x80U) {
            if (!template_literal_chars(rest[i])) {
                break;
            }
            ++i;
            continue;
        }
        std::size_t next = i;
        char32_t cp = 0;
        if (!decode_utf8(rest, next, cp) || !(is_ucschar(cp) || is_iprivate(cp))) {
            break;
        }
        i = next;
    }
    if (i == 0) {
        return urlg::error::mismatch;
    }
    char const* const first = it;
    it += i;
    return grammar::text(first, i);
}

// RFC 6570 2.2 and 2.3's expression, written with Boost.URL's combinators, one rule per
// production, and repeat_rule where the ABNF repeats: it yields text where range_rule would build a
// type-erased range for every varname, prefix and variable list.

/** varchar's run: letters, digits, "_" and pct-encoded triplets, at least one. */
inline constexpr auto varchars_rule =
    urlg::not_empty_rule(boost::urls::pct_encoded_rule(varchar_chars));

/** A dot, then a run of varchars: a varname's dot is always followed by a varchar. */
inline constexpr auto dotted_varchars_rule =
    urlg::tuple_rule(urlg::squelch(urlg::delim_rule('.')), varchars_rule);

/** varname = varchar *( ["."] varchar ). */
inline constexpr auto varname_rule = repeat_rule(varchars_rule, dotted_varchars_rule, 1);

/** One decimal digit. */
inline constexpr auto digit_rule = urlg::delim_rule(urlg::digit_chars);

/** max-length = %x31-39 0*3DIGIT: 1 to 9999, no leading zero. */
inline constexpr auto max_length_rule = urlg::tuple_rule(
    urlg::delim_rule(urlg::lut_chars("123456789")), repeat_rule(digit_rule, digit_rule, 0, 3));

/** modifier-level4 = prefix / explode, where prefix = ":" max-length and explode = "*". */
inline constexpr auto modifier_rule = urlg::variant_rule(
    urlg::tuple_rule(urlg::squelch(urlg::delim_rule(':')), max_length_rule), urlg::delim_rule('*'));

/** varspec = varname [ modifier-level4 ]. */
inline constexpr auto varspec_rule =
    urlg::tuple_rule(varname_rule, urlg::optional_rule(modifier_rule));

/** A comma, then a varspec: the rest of a variable-list. */
inline constexpr auto next_varspec_rule =
    urlg::tuple_rule(urlg::squelch(urlg::delim_rule(',')), varspec_rule);

/** expression = "{" [ operator ] variable-list "}", variable-list = varspec *( "," varspec ). */
inline constexpr auto expression_shape = urlg::tuple_rule(
    urlg::squelch(urlg::delim_rule('{')),
    urlg::optional_rule(urlg::delim_rule(template_operator_chars)),
    repeat_rule(varspec_rule, next_varspec_rule, 1), urlg::squelch(urlg::delim_rule('}')));

/**
 RFC 6570's expression as a terminal of the template: mismatch when no "{" is there, invalid when
 one starts and breaks.
*/
struct template_expression_rule_t {
    /** The rule yields the expression it matched, braces included. */
    using value_type = grammar::text;

    /** Matches an expression at `it`, or leaves `it`. */
    grammar::result<grammar::text> parse(char const*& it, char const* end) const noexcept;
};

// parse reads no member, yet stays a const member: Boost.URL's Rule concept calls it on the rule.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline grammar::result<grammar::text> template_expression_rule_t::parse(
    char const*& it, char const* end) const noexcept {
    if (it == end || *it != '{') {
        return urlg::error::mismatch;
    }
    char const* const first = it;
    if (const auto expression = urlg::parse(it, end, expression_shape); !expression) {
        it = first;
        return urlg::error::invalid;
    }
    return grammar::text(first, static_cast<std::size_t>(it - first));
}

/** A part of a template: literals or an expression. */
inline constexpr auto template_part_rule =
    urlg::variant_rule(template_literals_rule_t{}, template_expression_rule_t{});

/**
 RFC 6570 2's URI-Template, *( literals / expression ), its part passed by reference so the range
 keeps it inline.
*/
inline constexpr auto template_rule = urlg::range_rule(urlg::ref(template_part_rule));

}  // namespace detail

namespace grammar {

/**
 The rule of RFC 6570 section 2's URI-Template, with erratum 6937: literals and expressions.

 A literal is a printable ASCII character but the space, the double quote, the percent sign, the
 less-than and greater-than signs, the backslash, the circumflex, the grave accent, the braces and
 the vertical bar (the apostrophe is one, by the erratum), or a pct-encoded triplet, or a
 `ucschar` or `iprivate` code point. An expression is an opening brace, an optional operator (the
 reserved ones of `op-reserve` included, as the ABNF admits them), and varspecs separated by
 commas, each a varname with a colon and a max-length of 1 to 9999, or an asterisk, then a closing
 brace. The empty template is valid. The rule stops where the template does; an expression or a
 percent sign that starts and breaks makes it fail.

 @see "URI templates", in the guide.
*/
struct uri_template_rule_t {
    /** The rule yields the text it matched. */
    using value_type = text;

    /**
     Matches a URI-Template at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The text matched, possibly empty; `error::invalid` when an expression or a
     pct-encoded triplet starts and breaks.
    */
    [[nodiscard]] result<text> parse(char const*& it, char const* end) const noexcept;
};

/** The rule of RFC 6570's URI-Template. */
inline constexpr uri_template_rule_t uri_template_rule{};

// parse reads no member, yet stays a const member: Boost.URL's Rule concept calls it on the rule.
// Nothing escapes it, which the analysis cannot see: the one throw it finds is result::value(),
// whose system_error comes only from a result that holds an error, and Boost.URL's variant_rule
// and optional_rule read a result through operator* only after testing it; no range_rule here
// leaves any_rule's inline buffer, so nothing allocates either.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static,bugprone-exception-escape)
inline result<text> uri_template_rule_t::parse(char const*& it, char const* end) const noexcept {
    char const* const first = it;
    if (const auto parts = detail::urlg::parse(it, end, detail::template_rule); !parts) {
        it = first;
        return parts.error();
    }
    if (it != end && (*it == '{' || *it == '%')) {
        it = first;
        return detail::urlg::error::invalid;
    }
    return text(first, static_cast<std::size_t>(it - first));
}

}  // namespace grammar

/**
 Whether `s` is an RFC 6570 URI Template of any level, JSON Schema's `uri-template`.

 @param s Any byte string.
 @return `parse(s, grammar::uri_template_rule).has_value()`.
 @see "URI templates", in the guide.
*/
[[nodiscard]] inline bool is_uri_template(std::string_view s) noexcept {
    return detail::urlg::parse(s, grammar::uri_template_rule).has_value();
}

}  // namespace webcpp::lexical

#endif  // WEBCPP_LEXICAL_URI_TEMPLATE_HPP
