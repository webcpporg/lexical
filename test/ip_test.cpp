// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// IPv4 (RFC 2673 3.2's dotted quad, RFC 3986's dec-octet) and IPv6 (RFC 4291 2.2), through
// Boost.URL's rules: RFC 4291 2.2's examples, leading zeros, zone ids and prefixes refused.

#include <webcpp/lexical/ip.hpp>

#include <boost/core/lightweight_test.hpp>

#include <array>
#include <iostream>
#include <string_view>

namespace lexical = webcpp::lexical;

namespace {

void ipv4(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(lexical::is_ipv4(text), valid)) {
        std::cerr << "ipv4: \"" << text << "\"\n";
    }
}

void ipv6(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(lexical::is_ipv6(text), valid)) {
        std::cerr << "ipv6: \"" << text << "\"\n";
    }
}

void dotted_quads() {
    ipv4("192.168.0.1", true);
    ipv4("0.0.0.0", true);
    ipv4("255.255.255.255", true);
    ipv4("256.0.0.1", false);
    ipv4("192.168.0.01", false);
    ipv4("087.10.0.1", false);
    ipv4("1.2.3", false);
    ipv4("1.2.3.4.5", false);
    ipv4("0x7f000001", false);
    ipv4("1.2.3.4 ", false);
    ipv4(std::string_view("1.2.3.4\0", 8), false);
    // ARABIC-INDIC DIGIT ONE, a digit outside ASCII.
    ipv4("\xD9\xA1.2.3.4", false);
}

void rfc_4291_section_2_2() {
    constexpr std::array<std::string_view, 10> examples{
        "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789",  // the preferred form
        "2001:DB8:0:0:8:800:200C:417A",             //
        "2001:DB8::8:800:200C:417A",                // compressed zeros
        "FF01::101",                                //
        "::1",                                      //
        "::",                                       //
        "0:0:0:0:0:0:13.1.68.3",                    // an IPv4 address at the end
        "0:0:0:0:0:FFFF:129.144.52.38",             //
        "::13.1.68.3",                              // compressed, then an IPv4 address
        "::FFFF:129.144.52.38",                     //
    };
    for (const std::string_view text : examples) {
        ipv6(text, true);
    }
}

void ipv6_refusals() {
    ipv6("fe80::1%eth0", false);
    ipv6("2001:db8::/32", false);
    ipv6("1:2:3:4:5:6:7:8:9", false);
    ipv6("1::2::3", false);
    ipv6("::ffff:192.168.0.01", false);
    ipv6("12345::", false);
    ipv6("[::1]", false);
    ipv6(std::string_view("::1\0", 4), false);
}

}  // namespace

int main() {
    dotted_quads();
    rfc_4291_section_2_2();
    ipv6_refusals();
    return boost::report_errors();
}
