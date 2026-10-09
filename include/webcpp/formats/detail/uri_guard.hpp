// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The guard over Boost.URL 1.92's uri_reference_rule, which accepts a relative reference whose
// path after an authority is not path-abempty. It leaves with the first Boost release that refuses
// those references itself (test/uri_test.cpp, boost_url_still_needs_the_guard).

#ifndef WEBCPP_FORMATS_DETAIL_URI_GUARD_HPP
#define WEBCPP_FORMATS_DETAIL_URI_GUARD_HPP

#include <webcpp/formats/config.hpp>

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

}  // namespace webcpp::formats::detail

#endif  // WEBCPP_FORMATS_DETAIL_URI_GUARD_HPP
