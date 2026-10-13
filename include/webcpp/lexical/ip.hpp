// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 IP addresses, as validators over Boost.URL's own rules, `boost::urls::ipv4_address_rule` and
 `boost::urls::ipv6_address_rule`, which are lexical's rules too.

 @see "Addresses", in the guide.
*/
#ifndef WEBCPP_LEXICAL_IP_HPP
#define WEBCPP_LEXICAL_IP_HPP

#include <webcpp/lexical/config.hpp>

#include <boost/url/grammar/parse.hpp>
#include <boost/url/rfc/ipv4_address_rule.hpp>
#include <boost/url/rfc/ipv6_address_rule.hpp>

#include <string_view>

namespace webcpp::lexical {

/**
 Whether `s` is an IPv4 address in dotted-quad form, JSON Schema's `ipv4`.

 RFC 2673 3.2's dotted quad, each part RFC 3986's dec-octet, 0 to 255 without a leading zero.

 @param s Any byte string.
 @return `parse(s, boost::urls::ipv4_address_rule).has_value()`.
 @see "Addresses", in the guide.
*/
[[nodiscard]] inline bool is_ipv4(std::string_view s) noexcept {
    return boost::urls::grammar::parse(s, boost::urls::ipv4_address_rule).has_value();
}

/**
 Whether `s` is an IPv6 address, JSON Schema's `ipv6`.

 RFC 4291 2.2's three text forms; no zone id, no prefix length.

 @param s Any byte string.
 @return `parse(s, boost::urls::ipv6_address_rule).has_value()`.
 @see "Addresses", in the guide.
*/
[[nodiscard]] inline bool is_ipv6(std::string_view s) noexcept {
    return boost::urls::grammar::parse(s, boost::urls::ipv6_address_rule).has_value();
}

}  // namespace webcpp::lexical

#endif  // WEBCPP_LEXICAL_IP_HPP
