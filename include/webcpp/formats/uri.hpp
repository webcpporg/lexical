// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 URIs and URI references (RFC 3986), as validators and Boost.URL rules: `uri` is Boost.URL's own
 rule, `boost::urls::uri_rule`, and `uri-reference` Boost.URL's with a guard of formats'.

 @see "URIs and IRIs", in the guide.
*/
#ifndef WEBCPP_FORMATS_URI_HPP
#define WEBCPP_FORMATS_URI_HPP

#include <webcpp/formats/config.hpp>

#include <webcpp/formats/detail/uri_guard.hpp>
#include <webcpp/formats/grammar.hpp>

#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/rfc/uri_reference_rule.hpp>
#include <boost/url/url_view.hpp>

#include <string_view>

namespace webcpp::formats {

namespace detail {

namespace urlg = boost::urls::grammar;

}  // namespace detail

namespace grammar {

/**
 The rule of RFC 3986 4.1's URI-reference: Boost.URL's `uri_reference_rule`, then a guard.

 Boost.URL 1.92 accepts a relative reference whose path after the authority is not
 `path-abempty` (RFC 3986 4.2): `//example.com:abc/p`, whose port is not a number, and an
 authority that holds two at signs; the guard refuses them. It yields Boost.URL's `url_view` of
 the reference, and stops where Boost.URL's rule does.

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
     `error::invalid` when the guard does.
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
    if (detail::refuses_relative_authority_path(reference.unsafe_value())) {
        it = first;
        return detail::urlg::error::invalid;
    }
    return reference;
}

}  // namespace grammar

/**
 Whether `s` is an RFC 3986 URI, JSON Schema's `uri`: a scheme, and a fragment allowed.

 @param s Any byte string.
 @return `boost::urls::parse_uri(s).has_value()`.
 @see "URIs and IRIs", in the guide.
*/
[[nodiscard]] inline bool is_uri(std::string_view s) noexcept {
    return boost::urls::parse_uri(s).has_value();
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

}  // namespace webcpp::formats

#endif  // WEBCPP_FORMATS_URI_HPP
