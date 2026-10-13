// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Host names, RFC 1123's and internationalized ones, with their A-labels and their Unicode judged by
 webcpp::idna, as validators and Boost.URL rules.

 @see "Addresses", in the guide.
*/
#ifndef WEBCPP_LEXICAL_HOSTNAME_HPP
#define WEBCPP_LEXICAL_HOSTNAME_HPP

#include <webcpp/lexical/config.hpp>

#include <webcpp/lexical/detail/charsets.hpp>
#include <webcpp/lexical/detail/idn_scan.hpp>
#include <webcpp/lexical/grammar.hpp>

#include <webcpp/idna/options.hpp>
#include <webcpp/idna/uts46.hpp>
#include <webcpp/idna/validity.hpp>

#include <boost/system/error_code.hpp>
#include <boost/url/grammar/ci_string.hpp>
#include <boost/url/grammar/delim_rule.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/range_rule.hpp>
#include <boost/url/grammar/token_rule.hpp>
#include <boost/url/grammar/tuple_rule.hpp>

#include <array>
#include <cstddef>
#include <string_view>

namespace webcpp::lexical {

namespace detail {

namespace urlg = boost::urls::grammar;

/** Whether `label` starts with "xn--" in any case: an A-label (RFC 5890 2.3.2.1). */
constexpr bool has_ace_prefix(std::string_view label) noexcept {
    return label.size() >= 4 && (label[0] == 'x' || label[0] == 'X') &&
           (label[1] == 'n' || label[1] == 'N') && label[2] == '-' && label[3] == '-';
}

/** A label's characters, RFC 1123 2.1's letters, digits and hyphens, at least one. */
inline constexpr auto ldh_label_rule = urlg::token_rule(ldh_chars);

/** A dot, then a label: a name's dot is always between two labels. */
inline constexpr auto dotted_label_rule =
    urlg::tuple_rule(urlg::squelch(urlg::delim_rule('.')), ldh_label_rule);

/**
 A host name's labels, label *( "." label ), their nested rules by reference so the range keeps
 them inline.
*/
inline constexpr auto labels_rule =
    urlg::range_rule(urlg::ref(ldh_label_rule), urlg::ref(dotted_label_rule), 1);

/**
 Whether `label`, made of letters, digits and hyphens, has RFC 1123 2.1's length and ends: 1 to
 63 octets, no hyphen first or last.
*/
constexpr bool is_ldh_label(std::string_view label) noexcept {
    return !label.empty() && label.size() <= idna::max_label_length && label.front() != '-' &&
           label.back() != '-';
}

/**
 Why idn-hostname refuses `label`, an LDH A-label of at most 63 octets: idna's error for it
 lowercased, as UTS 46 maps it, or none.
*/
inline boost::system::error_code check_a_label(std::string_view label) noexcept {
    if (label.size() > idna::max_label_length) {
        return urlg::error::invalid;
    }
    std::array<char, idna::max_label_length> lower{};
    for (std::size_t i = 0; i < label.size(); ++i) {
        lower[i] = urlg::to_lower(label[i]);
    }
    if (const auto checked = idna::check_label(std::string_view(lower.data(), label.size()));
        !checked) {
        return checked.error();
    }
    return {};
}

/**
 Why `name`, a run of letters, digits, hyphens and dots, is no RFC 1123 host name, or none: its
 length, an empty label, a label's length or ends, or an A-label idna refuses.
*/
inline boost::system::error_code check_hostname(std::string_view name) noexcept {
    if (name.size() > idna::max_domain_length) {
        return urlg::error::invalid;
    }
    // The whole run as labels: a leading, trailing or double dot leaves input over (leftover).
    const auto labels = urlg::parse(name, labels_rule);
    if (!labels) {
        return urlg::error::invalid;
    }
    for (const grammar::text label : labels.unsafe_value()) {
        if (!is_ldh_label(label)) {
            return urlg::error::invalid;
        }
        if (has_ace_prefix(label)) {
            if (const boost::system::error_code refused = check_a_label(label)) {
                return refused;
            }
        }
    }
    return {};
}

/**
 Matches the IDN run of `kind` at `it` when `webcpp::idna::to_ascii`, with its default options,
 accepts it whole: mismatch when no run starts there, else idna's error.
*/
inline grammar::result<grammar::text> parse_idn_run(char const*& it, char const* end,
                                                    idn_run kind) noexcept {
    char const* const last = scan_idn_run(it, end, kind);
    if (last == it) {
        return urlg::error::mismatch;
    }
    const std::string_view run(it, static_cast<std::size_t>(last - it));
    std::array<char, idna::max_domain_length> ascii{};
    if (const auto converted = idna::to_ascii(ascii, run); !converted) {
        return converted.error();
    }
    char const* const first = it;
    it = last;
    return grammar::text(first, run.size());
}

}  // namespace detail

namespace grammar {

/**
 The rule of a host name: RFC 1123 2.1's labels, with each A-label judged as `idn-hostname`
 judges it.

 ASCII only. Labels of 1 to 63 letters, digits and hyphens, no hyphen first or last, joined by
 single dots, no dot first or last; the name at most 253 octets. A label that starts with `xn--`
 in any case is an A-label: lowercased and checked by `webcpp::idna::check_label` (RFC 5891 5.4:
 decoded, IDNA2008, the Bidi rule of the label alone, canonical). Another label with two hyphens
 in its third and fourth places is valid, as RFC 1123 says. It takes the run of letters, digits,
 hyphens and dots at its start and validates it whole, never a shorter prefix.

 @see "Addresses", in the guide.
*/
struct hostname_rule_t {
    /** The rule yields the text it matched. */
    using value_type = text;

    /**
     Matches a host name at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The text matched; `error::mismatch` when no letter, digit, hyphen or dot is at `it`;
     `error::invalid` when the run is no host name; idna's error when idna refuses an A-label.
    */
    [[nodiscard]] result<text> parse(char const*& it, char const* end) const noexcept;
};

/** The rule of a host name. */
inline constexpr hostname_rule_t hostname_rule{};

// A const member, as Boost.URL's Rule concept declares parse, though it reads no member.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline result<text> hostname_rule_t::parse(char const*& it, char const* end) const noexcept {
    char const* last = it;
    while (last != end && detail::hostname_chars(*last)) {
        ++last;
    }
    if (last == it) {
        return detail::urlg::error::mismatch;
    }
    const std::string_view name(it, static_cast<std::size_t>(last - it));
    if (const boost::system::error_code refused = detail::check_hostname(name)) {
        return refused;
    }
    char const* const first = it;
    it = last;
    return text(first, name.size());
}

/**
 The rule of an internationalized host name: UTS 46's processing and IDNA2008's validity, through
 `webcpp::idna::to_ascii`.

 idna's default options: UTS 46's mapping, nontransitional, every check, the DNS lengths, and
 IDNA2008's stricter validity (CONTEXTO, NV8 and XV8 refused). So fullwidth digits, an ignored
 U+200B, a name not in NFC and the dots U+3002, U+FF0E and U+FF61 are valid where IDNA2008 allows
 the result. It takes the run up to the first ASCII byte outside letters, digits, `-` and `.`
 (every non-ASCII byte belongs to it, since UTS 46 may map it) and validates it whole.

 @see "Addresses", in the guide.
*/
struct idn_hostname_rule_t {
    /** The rule yields the text it matched. */
    using value_type = text;

    /**
     Matches an internationalized host name at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The text matched; `error::mismatch` when no name's byte is at `it`; else idna's error
     when idna refuses the run.
    */
    [[nodiscard]] result<text> parse(char const*& it, char const* end) const noexcept;
};

/**
 The rule of one label of an internationalized host name, a terminal of lexical's own that asks
 `webcpp::idna` for its verdict.

 The run stops also at `.`, U+3002, U+FF0E and U+FF61, so a grammar composes labels with its own
 separators. The label is checked as @ref idn_hostname_rule checks a name of one label, alone: a
 grammar that joins labels into a name checks what spans the name itself, RFC 5893's Bidi rule
 across its labels (section 2) and its 253 octets in the ASCII form, or hands the whole name to
 @ref idn_hostname_rule, which checks both.

 @see "Composing rules", in the guide.
*/
struct idna_label_rule_t {
    /** The rule yields the text it matched. */
    using value_type = text;

    /**
     Matches one internationalized label at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The text matched; `error::mismatch` when no label starts at `it`; else idna's error.
    */
    [[nodiscard]] result<text> parse(char const*& it, char const* end) const noexcept;
};

/** The rule of an internationalized host name. */
inline constexpr idn_hostname_rule_t idn_hostname_rule{};

/** The rule of one internationalized label. */
inline constexpr idna_label_rule_t idna_label_rule{};

// A const member, as Boost.URL's Rule concept declares parse, though it reads no member.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline result<text> idn_hostname_rule_t::parse(char const*& it, char const* end) const noexcept {
    return detail::parse_idn_run(it, end, detail::idn_run::domain);
}

// A const member, as Boost.URL's Rule concept declares parse, though it reads no member.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline result<text> idna_label_rule_t::parse(char const*& it, char const* end) const noexcept {
    return detail::parse_idn_run(it, end, detail::idn_run::label);
}

}  // namespace grammar

/**
 Whether `s` is a host name, JSON Schema's `hostname`.

 @param s Any byte string.
 @return `parse(s, grammar::hostname_rule).has_value()`.
 @see "Addresses", in the guide.
*/
[[nodiscard]] inline bool is_hostname(std::string_view s) noexcept {
    return detail::urlg::parse(s, grammar::hostname_rule).has_value();
}

/**
 Whether `s` is an internationalized host name, JSON Schema's `idn-hostname`.

 @param s Any byte string.
 @return `parse(s, grammar::idn_hostname_rule).has_value()`.
 @see "Addresses", in the guide.
*/
[[nodiscard]] inline bool is_idn_hostname(std::string_view s) noexcept {
    return detail::urlg::parse(s, grammar::idn_hostname_rule).has_value();
}

}  // namespace webcpp::lexical

#endif  // WEBCPP_LEXICAL_HOSTNAME_HPP
