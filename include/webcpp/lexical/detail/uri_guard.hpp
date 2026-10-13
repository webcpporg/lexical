// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The guard over Boost.URL 1.92's uri_reference_rule, which accepts a relative reference whose
// path after an authority is not path-abempty, and stops a URI at an authority that the same
// characters follow. refuses_relative_authority_path leaves with the first Boost release that
// refuses those relative references itself (test/uri_test.cpp, boost_url_still_needs_the_guard);
// refuses_authority_before_segment stays, since that release stops them at the authority too.
// refuses_bracket_in_query covers Boost.URL 1.92's query characters, which hold "[" and "]"
// (boost/url/rfc/detail/charsets.hpp, query_chars); it leaves with the first Boost release that
// refuses them (test/uri_test.cpp, boost_url_still_takes_brackets_in_a_query).
// refuses_number_sign_in_fragment covers its fragment characters, which hold "#" (the same file,
// fragment_chars); it leaves with the first Boost release that refuses it (test/uri_test.cpp,
// boost_url_still_takes_a_number_sign_in_a_fragment). refuses_zone_identifier covers its IP
// literal, which takes RFC 6874's zone identifier
// (boost/url/rfc/detail/impl/ipv6_addrz_rule.hpp); it leaves with the first Boost release that
// refuses one (test/uri_test.cpp, boost_url_still_takes_a_zone_identifier).

#ifndef WEBCPP_LEXICAL_DETAIL_URI_GUARD_HPP
#define WEBCPP_LEXICAL_DETAIL_URI_GUARD_HPP

#include <webcpp/lexical/config.hpp>

#include <boost/core/detail/string_view.hpp>
#include <boost/url/host_type.hpp>
#include <boost/url/rfc/pchars.hpp>
#include <boost/url/url_view.hpp>

namespace webcpp::lexical::detail {

/**
 True when Boost.URL accepted a relative reference with an authority and a path that is not empty
 and not led by "/", which RFC 3986 4.2 refuses.
*/
inline bool refuses_relative_authority_path(boost::urls::url_view u) noexcept {
    return !u.has_scheme() && u.has_authority() && !u.encoded_path().empty() &&
           u.encoded_path().front() != '/';
}

/** The characters that start a path segment right after an authority: a pchar but the colon. */
inline constexpr auto segment_nc_chars = boost::urls::pchars - ':';

/**
 True when a reference ends at its authority, path, query and fragment empty, before a character
 that starts a path segment, the percent sign of a percent-encoded octet included.

 Tip: Boost.URL stops a URI there, at the second at sign of an authority that holds two, and
 reads a relative reference's rest as its path, which refuses_relative_authority_path refuses.
*/
inline bool refuses_authority_before_segment(boost::urls::url_view u, char const* next,
                                             char const* end) noexcept {
    if (next == end || !u.has_authority() || !u.encoded_path().empty() || u.has_query() ||
        u.has_fragment()) {
        return false;
    }
    return *next == '%' || segment_nc_chars(*next);
}

/**
 True when a reference holds "[" or "]" in its query, which RFC 3986 3.4 refuses: the two
 delimit an IP literal alone (3.2.2), and Boost.URL refuses them in a path and a fragment already.
*/
inline bool refuses_bracket_in_query(boost::urls::url_view u) noexcept {
    return u.has_query() && u.encoded_query().find_first_of("[]") != boost::core::string_view::npos;
}

/**
 True when a reference holds "#" in its fragment, which RFC 3986 3.5 refuses: the first "#" ends
 what precedes the fragment, and the fragment holds pchar, "/" and "?" alone.

 Tip: Boost.URL 1.92 adds "#" to its fragment characters on purpose (its commit "hash is fragment
 char"), so a second "#" stays in the fragment there.
*/
inline bool refuses_number_sign_in_fragment(boost::urls::url_view u) noexcept {
    return u.has_fragment() && u.encoded_fragment().contains('#');
}

/**
 True when a reference's IP literal holds a zone identifier, which RFC 3986 3.2.2 refuses: its
 IP-literal is an IPv6address or an IPvFuture, and the "%" of a zone identifier is neither.

 Tip: RFC 6874 added one to RFC 3986's IP-literal, which Boost.URL 1.92 takes; RFC 9844 obsoleted
 RFC 6874 and reverted that change.
*/
inline bool refuses_zone_identifier(boost::urls::url_view u) noexcept {
    return u.host_type() == boost::urls::host_type::ipv6 && u.encoded_host().contains('%');
}

}  // namespace webcpp::lexical::detail

#endif  // WEBCPP_LEXICAL_DETAIL_URI_GUARD_HPP
