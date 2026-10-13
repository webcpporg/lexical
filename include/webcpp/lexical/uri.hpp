// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 URIs and IRIs (RFC 3986, RFC 3987), as validators and Boost.URL rules: `uri` and `uri-reference`
 are Boost.URL's rules with a guard of lexical's, and the IRIs lexical's rules, which map an IRI to
 a URI that Boost.URL parses.

 @see "URIs and IRIs", in the guide.
*/
#ifndef WEBCPP_LEXICAL_URI_HPP
#define WEBCPP_LEXICAL_URI_HPP

#include <webcpp/lexical/config.hpp>

#include <webcpp/lexical/detail/iri_mapping.hpp>
#include <webcpp/lexical/detail/uri_guard.hpp>
#include <webcpp/lexical/grammar.hpp>

#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/rfc/uri_reference_rule.hpp>
#include <boost/url/rfc/uri_rule.hpp>
#include <boost/url/url_view.hpp>

#include <cstddef>
#include <memory_resource>
#include <string_view>

namespace webcpp::lexical {

namespace detail {

namespace urlg = boost::urls::grammar;

}  // namespace detail

namespace grammar {

/**
 The rule of RFC 3986 3's URI: Boost.URL's `uri_rule`, then a guard.

 Boost.URL 1.92 accepts "[" and "]" in a query, which RFC 3986 holds only around an IP literal,
 "#" in a fragment, which RFC 3986 3.5 refuses there, and RFC 6874's zone identifier in an IP
 literal, which RFC 9844 withdrew, and stops a URI at an authority that holds two at signs; the
 guard refuses each with `error::invalid`. Otherwise the rule yields Boost.URL's `url_view` of
 the URI, and stops where Boost.URL's rule does.

 @see "URIs and IRIs", in the guide.
*/
struct uri_rule_t {
    /** The rule yields Boost.URL's view of the URI it matched. */
    using value_type = boost::urls::url_view;

    /**
     Matches a URI at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The view of the URI; Boost.URL's error when its rule refuses the input;
     `error::invalid` when the guard does.
    */
    [[nodiscard]] result<boost::urls::url_view> parse(char const*& it,
                                                      char const* end) const noexcept;
};

/** The rule of RFC 3986's URI, guarded. */
inline constexpr uri_rule_t uri_rule{};

// parse reads no member, yet stays a const member: Boost.URL's Rule concept calls it on the rule.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline result<boost::urls::url_view> uri_rule_t::parse(char const*& it,
                                                       char const* end) const noexcept {
    char const* const first = it;
    auto uri = detail::urlg::parse(it, end, boost::urls::uri_rule);
    if (!uri) {
        it = first;
        return uri.error();
    }
    // Read with unsafe_value: the result holds a value here, and operator* may throw.
    const boost::urls::url_view view = uri.unsafe_value();
    if (detail::refuses_authority_before_segment(view, it, end) ||
        detail::refuses_bracket_in_query(view) || detail::refuses_number_sign_in_fragment(view) ||
        detail::refuses_zone_identifier(view)) {
        it = first;
        return detail::urlg::error::invalid;
    }
    return uri;
}

/**
 The rule of RFC 3986 4.1's URI-reference: Boost.URL's `uri_reference_rule`, then a guard.

 Boost.URL 1.92 accepts a relative reference whose path after the authority is not
 `path-abempty` (RFC 3986 4.2), as `//example.com:abc/p`, whose port is not a number, and an
 authority that holds two at signs; with a scheme, it stops the URI at that authority. It also
 accepts "[" and "]" in a query, which RFC 3986 holds only around an IP literal, "#" in a
 fragment, which RFC 3986 3.5 refuses there, and RFC 6874's zone identifier in an IP literal,
 which RFC 9844 withdrew. The guard refuses each with `error::invalid`. Otherwise the rule yields
 Boost.URL's `url_view` of the reference, and stops where Boost.URL's rule does.

 @see "URIs and IRIs", in the guide.
*/
struct uri_reference_rule_t {
    /** The rule yields Boost.URL's view of the reference it matched. */
    using value_type = boost::urls::url_view;

    /**
     Matches a URI-reference at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The view of the reference; Boost.URL's error when its rule refuses the input;
     `error::invalid` when the guard does, a reference that ends at its authority before a
     character that starts a path segment included.
    */
    [[nodiscard]] result<boost::urls::url_view> parse(char const*& it,
                                                      char const* end) const noexcept;
};

/** The rule of RFC 3986's URI-reference, guarded. */
inline constexpr uri_reference_rule_t uri_reference_rule{};

// parse reads no member, yet stays a const member: Boost.URL's Rule concept calls it on the rule.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline result<boost::urls::url_view> uri_reference_rule_t::parse(char const*& it,
                                                                 char const* end) const noexcept {
    char const* const first = it;
    auto reference = detail::urlg::parse(it, end, boost::urls::uri_reference_rule);
    if (!reference) {
        it = first;
        return reference.error();
    }
    // Read with unsafe_value: the result holds a value here, and operator* may throw.
    const boost::urls::url_view view = reference.unsafe_value();
    if (detail::refuses_relative_authority_path(view) ||
        detail::refuses_authority_before_segment(view, it, end) ||
        detail::refuses_bracket_in_query(view) || detail::refuses_number_sign_in_fragment(view) ||
        detail::refuses_zone_identifier(view)) {
        it = first;
        return detail::urlg::error::invalid;
    }
    return reference;
}

/**
 The rule of RFC 3987 2.2's IRI: an IRI checked code point by code point, mapped to a URI, and
 parsed by @ref uri_rule.

 A non-ASCII code point must be well-formed UTF-8 and in `ucschar`, or `iprivate` in the query;
 the bidirectional formatting characters are refused anywhere (RFC 3987 4.1), and so is any
 non-ASCII code point inside an IP literal, which RFC 3987 takes from RFC 3986, ASCII alone. Each
 octet of a non-ASCII code point maps to `%XX`, which RFC 3986 allows elsewhere where RFC 3987
 allows `ucschar`, so a non-ASCII scheme or port is refused too. The rule takes the run up to the
 first byte no IRI holds (an ASCII control, the space, the double quote, the less-than and
 greater-than signs, the backslash, the circumflex, the grave accent, the braces or the vertical
 bar) and validates it whole; it never matches a shorter prefix.

 @see "URIs and IRIs", in the guide.
*/
struct iri_rule_t {
    /** The rule yields the text it matched. */
    using value_type = text;

    /** The resource of a mapped URI longer than 1,024 octets; null for the default resource. */
    std::pmr::memory_resource* resource = nullptr;

    /**
     Matches an IRI at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The text matched; `error::mismatch` when a byte no IRI holds is at `it`;
     `error::invalid` when the run is no IRI.
     @throws std::bad_alloc When the resource cannot allocate, or what it throws instead.
    */
    [[nodiscard]] result<text> parse(char const*& it, char const* end) const;
};

/**
 The rule of RFC 3987 2.2's IRI-reference: an IRI or a relative reference, checked and mapped as
 @ref iri_rule_t does, then parsed by @ref uri_reference_rule.

 The empty reference is valid, so the rule matches empty text where a byte no IRI holds is at
 the start, and never answers mismatch.

 @see "URIs and IRIs", in the guide.
*/
struct iri_reference_rule_t {
    /** The rule yields the text it matched. */
    using value_type = text;

    /** The resource of a mapped URI longer than 1,024 octets; null for the default resource. */
    std::pmr::memory_resource* resource = nullptr;

    /**
     Matches an IRI-reference at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The text matched; `error::invalid` when the run is no IRI reference.
     @throws std::bad_alloc When the resource cannot allocate, or what it throws instead.
    */
    [[nodiscard]] result<text> parse(char const*& it, char const* end) const;
};

/** The rule of RFC 3987's IRI, with the default resource; `iri_rule_t{&resource}` names another. */
inline constexpr iri_rule_t iri_rule{};

/** The rule of RFC 3987's IRI-reference, with the default resource. */
inline constexpr iri_reference_rule_t iri_reference_rule{};

inline result<text> iri_rule_t::parse(char const*& it, char const* end) const {
    char const* const first = it;
    char const* const last = detail::scan_iri(first, end);
    if (last == first) {
        return detail::urlg::error::mismatch;
    }
    const std::string_view run(first, static_cast<std::size_t>(last - first));
    const bool valid = detail::with_mapped_iri(run, resource, [](std::string_view uri) {
        return detail::urlg::parse(uri, uri_rule).has_value();
    });
    if (!valid) {
        return detail::urlg::error::invalid;
    }
    it = last;
    return text(first, run.size());
}

inline result<text> iri_reference_rule_t::parse(char const*& it, char const* end) const {
    char const* const first = it;
    char const* const last = detail::scan_iri(first, end);
    const std::string_view run(first, static_cast<std::size_t>(last - first));
    const bool valid = detail::with_mapped_iri(run, resource, [](std::string_view uri) {
        return detail::urlg::parse(uri, uri_reference_rule).has_value();
    });
    if (!valid) {
        return detail::urlg::error::invalid;
    }
    it = last;
    return text(first, run.size());
}

}  // namespace grammar

/**
 Whether `s` is an RFC 3986 URI, JSON Schema's `uri`: a scheme, and a fragment allowed.

 @param s Any byte string.
 @return `parse(s, grammar::uri_rule).has_value()`.
 @see "URIs and IRIs", in the guide.
*/
[[nodiscard]] inline bool is_uri(std::string_view s) noexcept {
    return detail::urlg::parse(s, grammar::uri_rule).has_value();
}

/**
 Whether `s` is an RFC 3986 URI-reference, JSON Schema's `uri-reference`: a URI or a relative
 reference.

 @param s Any byte string.
 @return `parse(s, grammar::uri_reference_rule).has_value()`.
 @see "URIs and IRIs", in the guide.
*/
[[nodiscard]] inline bool is_uri_reference(std::string_view s) noexcept {
    return detail::urlg::parse(s, grammar::uri_reference_rule).has_value();
}

/**
 Whether `s` is an RFC 3987 IRI, JSON Schema's `iri`.

 @param s Any byte string.
 @return `parse(s, grammar::iri_rule).has_value()`.
 @throws std::bad_alloc When the default resource cannot allocate, only for an IRI whose mapped URI
 exceeds 1,024 octets.
 @see "URIs and IRIs", in the guide.
*/
[[nodiscard]] inline bool is_iri(std::string_view s) {
    return detail::urlg::parse(s, grammar::iri_rule).has_value();
}

/**
 Whether `s` is an RFC 3987 IRI, a mapped URI beyond 1,024 octets taken from `resource`.

 @param s Any byte string.
 @param resource Where the mapped copy of a long non-ASCII IRI is allocated; an ASCII IRI never
 reaches it.
 @return `parse(s, grammar::iri_rule_t{&resource}).has_value()`.
 @throws std::bad_alloc When `resource` cannot allocate, or what it throws instead.
 @see "URIs and IRIs", in the guide.
*/
[[nodiscard]] inline bool is_iri(std::string_view s, std::pmr::memory_resource& resource) {
    return detail::urlg::parse(s, grammar::iri_rule_t{&resource}).has_value();
}

/**
 Whether `s` is an RFC 3987 IRI-reference, JSON Schema's `iri-reference`.

 @param s Any byte string.
 @return `parse(s, grammar::iri_reference_rule).has_value()`.
 @throws std::bad_alloc When the default resource cannot allocate, only for a reference whose mapped
 URI exceeds 1,024 octets.
 @see "URIs and IRIs", in the guide.
*/
[[nodiscard]] inline bool is_iri_reference(std::string_view s) {
    return detail::urlg::parse(s, grammar::iri_reference_rule).has_value();
}

/**
 Whether `s` is an RFC 3987 IRI-reference, a mapped URI beyond 1,024 octets taken from `resource`.

 @param s Any byte string.
 @param resource Where the mapped copy of a long non-ASCII reference is allocated; an ASCII
 reference never reaches it.
 @return `parse(s, grammar::iri_reference_rule_t{&resource}).has_value()`.
 @throws std::bad_alloc When `resource` cannot allocate, or what it throws instead.
 @see "URIs and IRIs", in the guide.
*/
[[nodiscard]] inline bool is_iri_reference(std::string_view s,
                                           std::pmr::memory_resource& resource) {
    return detail::urlg::parse(s, grammar::iri_reference_rule_t{&resource}).has_value();
}

}  // namespace webcpp::lexical

#endif  // WEBCPP_LEXICAL_URI_HPP
