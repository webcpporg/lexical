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

#ifndef WEBCPP_FORMATS_DETAIL_URI_GUARD_HPP
#define WEBCPP_FORMATS_DETAIL_URI_GUARD_HPP

#include <webcpp/formats/config.hpp>

#include <boost/url/rfc/pchars.hpp>
#include <boost/url/url_view.hpp>

namespace webcpp::formats::detail {

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

}  // namespace webcpp::formats::detail

#endif  // WEBCPP_FORMATS_DETAIL_URI_GUARD_HPP
