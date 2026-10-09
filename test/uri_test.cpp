// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// RFC 3986's URI and URI reference through Boost.URL: section 1.1.2's examples, the guard's two
// inputs refused (and the same with a scheme), the zone id verdict Boost.URL gives, and a pin of
// Boost.URL's own reading of the two inputs, which fails the day a Boost release fixes it, when
// the guard can leave.

#include <webcpp/formats/uri.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/url/grammar/delim_rule.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/tuple_rule.hpp>
#include <boost/url/parse.hpp>

#include <array>
#include <iostream>
#include <string_view>
#include <tuple>

namespace formats = webcpp::formats;
namespace fg = webcpp::formats::grammar;
namespace urlg = boost::urls::grammar;

namespace {

void uri(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(formats::is_uri(text), valid)) {
        std::cerr << "uri: \"" << text << "\"\n";
    }
}

/** Checks `text` against `valid`, and that the function and the rule agree. */
void reference(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(formats::is_uri_reference(text), valid)) {
        std::cerr << "uri-reference: \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(formats::is_uri_reference(text),
                  urlg::parse(text, fg::uri_reference_rule).has_value());
}

void rfc_3986_section_1_1_2() {
    constexpr std::array<std::string_view, 8> examples{
        "ftp://ftp.is.co.za/rfc/rfc1808.txt",                   //
        "http://www.ietf.org/rfc/rfc2396.txt",                  //
        "ldap://[2001:db8::7]/c=GB?objectClass?one",            // an IPv6 host, a query
        "mailto:John.Doe@example.com",                          // no authority
        "news:comp.infosystems.www.servers.unix",               //
        "tel:+1-816-555-1212",                                  //
        "telnet://192.0.2.16:80/",                              // an IPv4 host, a port
        "urn:oasis:names:specification:docbook:dtd:xml:4.1.2",  //
    };
    for (const std::string_view text : examples) {
        uri(text, true);
        reference(text, true);
    }
}

void uris_and_references() {
    // RFC 3986 3's URI allows a fragment, where 4.3's absolute-URI does not.
    uri("http://example.com/p?q#fragment", true);
    uri("urn:example:a#", true);
    uri("//example.com/p", false);
    uri("/abc", false);
    uri("http://example.com/a b", false);
    uri("http://example.com/%zz", false);
    uri(std::string_view("http://a/\0", 10), false);
    reference("//example.com/p", true);
    reference("", true);
    reference("#fragment", true);
    reference("../a/b?c", true);
    reference(R"(\\WINDOWS\fileshare)", false);
    reference("http://example.com/a b", false);
    reference(std::string_view("//a/\0", 5), false);
}

void the_guard() {
    // RFC 3986 4.2: a relative reference with an authority has a path-abempty, empty or led by
    // "/". Boost.URL 1.92 reads these two as an authority and a path that is neither.
    reference("//example.com:abc/p", false);
    reference("//a@b@example.com/", false);
    reference("//:abc/p", false);
    uri("http://example.com:abc/p", false);
    uri("http://a@b@example.com/", false);
    reference("http://example.com:abc/p", false);
    reference("http://a@b@example.com/", false);
    reference("//example.com:80/p", true);
    reference("//a@example.com", true);
    reference("//example.com", true);
    reference("//example.com?q", true);
    reference("//example.com#f", true);
}

void boost_url_still_needs_the_guard() {
    // When this fails, Boost.URL refuses the input itself: the guard and its changelog line
    // leave, in the change that moves to that Boost release, and the_guard stays.
    const auto port = boost::urls::parse_uri_reference("//example.com:abc/p");
    if (BOOST_TEST(port.has_value())) {
        BOOST_TEST_EQ(port->encoded_host(), "example.com");
        BOOST_TEST_EQ(port->encoded_path(), "abc/p");
    }
    const auto at = boost::urls::parse_uri_reference("//a@b@example.com/");
    if (BOOST_TEST(at.has_value())) {
        BOOST_TEST_EQ(at->encoded_path(), "@example.com/");
    }
}

void zone_ids_keep_boost_urls_verdict() {
    // RFC 6874's zone id, which Boost.URL accepts and the suite does not test (the page's
    // "Known deviations").
    uri("http://[fe80::1%25eth0]/", true);
    reference("//[fe80::1%25eth0]/", true);
}

void the_rule_inside_a_grammar() {
    const std::string_view text = "//a@b@example.com/";
    char const* it = text.data();
    const auto r = fg::uri_reference_rule.parse(it, text.data() + text.size());
    BOOST_TEST(r.has_error() && r.error() == urlg::error::invalid);
    BOOST_TEST(it == text.data());
    // Boost.URL's rule leaves its iterator past what it read when it refuses; the rule does not.
    const std::string_view broken = "a/%zz";
    char const* at = broken.data();
    const auto refused = fg::uri_reference_rule.parse(at, broken.data() + broken.size());
    BOOST_TEST(refused.has_error());
    BOOST_TEST(at == broken.data());
    const auto pair = urlg::parse(
        "a/b c/d", urlg::tuple_rule(fg::uri_reference_rule, urlg::squelch(urlg::delim_rule(' ')),
                                    fg::uri_reference_rule));
    if (BOOST_TEST(pair.has_value())) {
        BOOST_TEST_EQ(std::get<0>(*pair).buffer(), "a/b");
        BOOST_TEST_EQ(std::get<1>(*pair).buffer(), "c/d");
    }
}

}  // namespace

int main() {
    rfc_3986_section_1_1_2();
    uris_and_references();
    the_guard();
    boost_url_still_needs_the_guard();
    zone_ids_keep_boost_urls_verdict();
    the_rule_inside_a_grammar();
    return boost::report_errors();
}
