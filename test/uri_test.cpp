// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// RFC 3986's URI and URI reference through Boost.URL: section 1.1.2's examples, the guard's inputs
// refused (and the same with a scheme), alone and inside a larger grammar, a bracket in a query,
// a number sign in a fragment and a zone id refused, the IPvFuture verdict Boost.URL gives, and
// pins of Boost.URL's own reading of what the guard refuses, each failing the day a Boost release
// fixes it, when its part of the guard can leave.

#include <webcpp/lexical/uri.hpp>

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

namespace lexical = webcpp::lexical;
namespace fg = webcpp::lexical::grammar;
namespace urlg = boost::urls::grammar;

namespace {

/** Checks `text` against `valid`, and that the function and the rule agree. */
void uri(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(lexical::is_uri(text), valid)) {
        std::cerr << "uri: \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(lexical::is_uri(text), urlg::parse(text, fg::uri_rule).has_value());
}

/** Checks `text` against `valid`, and that the function and the rule agree. */
void reference(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(lexical::is_uri_reference(text), valid)) {
        std::cerr << "uri-reference: \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(lexical::is_uri_reference(text),
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
    // "/". Boost.URL 1.92 reads each of these as an authority and a path that is neither: after a
    // port that is not a number, a second at sign, or an IP literal.
    reference("//example.com:abc/p", false);
    reference("//a@b@example.com/", false);
    reference("//:abc/p", false);
    reference("//example.com:abc", false);
    reference("//h:1a/p", false);
    reference("//[::1]x/p", false);
    uri("http://example.com:abc/p", false);
    uri("http://a@b@example.com/", false);
    reference("http://example.com:abc/p", false);
    reference("http://a@b@example.com/", false);
    reference("//example.com:80/p", true);
    reference("//a@example.com", true);
    reference("//example.com", true);
    reference("//example.com?q", true);
    reference("//example.com#f", true);
    reference("//example.com:/p", true);
    reference("///p", true);
    reference("//", true);
}

void brackets_only_around_an_ip_literal() {
    // RFC 3986 3.2.2: "[" and "]" delimit an IP literal and nothing else; a query (3.4), a path
    // (3.3) and a fragment (3.5) hold them only percent-encoded. Boost.URL 1.92 takes them in a
    // query.
    constexpr std::array<std::string_view, 8> refused{
        "http://a/?[x]",    // the query
        "http://a/?]",      //
        "http://a/?a=[1]",  //
        "urn:a?[x]",        //
        "http://a/[x]",     // the path
        "http://a/p]q",     //
        "http://a/#[x]",    // the fragment
        "http://[x]@a/",    // the user information
    };
    for (const std::string_view text : refused) {
        uri(text, false);
        reference(text, false);
    }
    constexpr std::array<std::string_view, 5> relative{
        "?[x]",      //
        "a?[x]",     //
        "//h?]",     //
        "/p?a=[1]",  //
        "#[x]",      //
    };
    for (const std::string_view text : relative) {
        reference(text, false);
    }
    uri("http://[::1]/", true);
    uri("http://[::1]:80/p?q#f", true);
    uri("http://a/?%5Bx%5D", true);
    uri("http://[::1]/?%5B%5D#%5D", true);
    reference("//[::1]/p", true);
    reference("?%5B", true);
}

void one_number_sign_before_the_fragment() {
    // RFC 3986 3.5: fragment = *( pchar / "/" / "?" ). The first "#" starts the fragment, which
    // holds no other but percent-encoded, as "%23". Boost.URL 1.92 takes "#" in a fragment.
    constexpr std::array<std::string_view, 6> refused{
        "http://a/#x#y",     //
        "http://a/#x#",      //
        "http://a/##",       //
        "http://a/?q#x#y",   // after a query
        "urn:a##",           //
        "http://[::1]/#?#",  // after a "?" in the fragment
    };
    for (const std::string_view text : refused) {
        uri(text, false);
        reference(text, false);
    }
    constexpr std::array<std::string_view, 5> relative{
        "#x#y",     //
        "##",       //
        "a#x#y",    //
        "//h/#x#",  //
        "?q##",     //
    };
    for (const std::string_view text : relative) {
        reference(text, false);
    }
    uri("http://a/#x%23y", true);
    uri("http://a/#%23", true);
    uri("http://a/?q#x?y/z", true);
    uri("http://a/?q%23#x%23", true);
    reference("#x%23y", true);
    reference("#", true);
    reference("a?%23#%23", true);
}

void boost_url_still_takes_a_number_sign_in_a_fragment() {
    // When this fails, Boost.URL refuses it itself, and the fragment guard may leave in a change of
    // its own, with a changelog line; one_number_sign_before_the_fragment stays.
    const auto u = boost::urls::parse_uri("http://a/#x#y");
    if (BOOST_TEST(u.has_value())) {
        BOOST_TEST_EQ(u->encoded_fragment(), "x#y");
    }
    BOOST_TEST(boost::urls::parse_uri_reference("#x#y").has_value());
}

void boost_url_still_takes_brackets_in_a_query() {
    // When this fails, Boost.URL refuses them itself, and the bracket guard may leave in a change
    // of its own, with a changelog line; brackets_only_around_an_ip_literal stays.
    BOOST_TEST(boost::urls::parse_uri("http://a/?[x]").has_value());
    BOOST_TEST(boost::urls::parse_uri_reference("?[x]").has_value());
}

void boost_url_still_needs_the_guard() {
    // When this fails, Boost.URL refuses the input itself: refuses_relative_authority_path and its
    // changelog line leave, in the change that moves to that Boost release, and the_guard stays.
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

void no_zone_identifier() {
    // RFC 3986 3.2.2's IP-literal holds an IPv6address or an IPvFuture, and no zone identifier:
    // RFC 9844 obsoleted RFC 6874, which had added one, and reverted its change to RFC 3986.
    // Boost.URL 1.92 takes RFC 6874's.
    constexpr std::array<std::string_view, 5> refused{
        "http://[fe80::1%25eth0]/",     //
        "http://[::1%25a]:80/p?q#f",    //
        "http://u@[fe80::1%25%41]/",    // a percent-encoded zone identifier
        "http://[::ffff:1.2.3.4%251]",  // after an IPv4 tail
        "urn://[::1%25x]",              //
    };
    for (const std::string_view text : refused) {
        uri(text, false);
        reference(text, false);
    }
    reference("//[fe80::1%25eth0]/", false);
    reference("//[::1%25x]", false);
    uri("http://[fe80::1]/", true);
    uri("http://[::ffff:1.2.3.4]/%25", true);  // "%25" outside the IP literal
    uri("http://%25@h/?%25#%25", true);
    reference("//[::1]/%25", true);
    // A percent sign elsewhere in an authority whose host is an IPv6 address.
    uri("http://%25@[::1]:80/", true);
    uri("http://u%41:p%25@[::1]/p?%25", true);
    reference("//a%25b@[fe80::1]/%25", true);
    reference("//a%25b@[fe80::1]:8080?q%25", true);
}

void boost_url_still_takes_a_zone_identifier() {
    // When this fails, Boost.URL refuses it itself, and the zone guard may leave in a change of its
    // own, with a changelog line; no_zone_identifier stays.
    const auto u = boost::urls::parse_uri("http://[fe80::1%25eth0]/");
    if (BOOST_TEST(u.has_value())) {
        BOOST_TEST_EQ(u->encoded_host(), "[fe80::1%25eth0]");
    }
    BOOST_TEST(boost::urls::parse_uri_reference("//[::1%25x]").has_value());
}

void ipvfuture_keeps_boost_urls_verdict() {
    // RFC 3986 3.2.2's IPvFuture starts with "v", which RFC 5234 2.3 makes case-insensitive, as
    // every quoted string of ABNF; Boost.URL 1.92 refuses "V" (the page's "Known deviations"), and
    // lexical does not loosen Boost.URL. When this fails, Boost.URL takes "V" itself: the two false
    // verdicts become true, RFC 3986's, as does iri_test's on "http://[V1.x]/", and the deviation
    // leaves the page and AGENTS.md.
    uri("http://[V1.x]/", false);
    reference("//[V1.x]/", false);
    uri("http://[v1.x]/", true);
    uri("http://[vF.x:y]/", true);
    reference("//[v1a.!$]/", true);
}

/** Checks that the rule refuses `text` as invalid, its iterator unchanged. */
void invalid_inside_a_grammar(std::string_view text) {
    char const* it = text.data();
    const auto r = fg::uri_reference_rule.parse(it, text.data() + text.size());
    if (!BOOST_TEST(r.has_error() && r.error() == urlg::error::invalid)) {
        std::cerr << "uri_reference_rule: \"" << text << "\"\n";
    }
    BOOST_TEST(it == text.data());
}

/** Checks that the rule matches `match`, a prefix of `text`, and stops after it. */
void matches_inside_a_grammar(std::string_view text, std::string_view match) {
    char const* it = text.data();
    const auto r = fg::uri_reference_rule.parse(it, text.data() + text.size());
    if (!BOOST_TEST(r.has_value() && r.unsafe_value().buffer() == match)) {
        std::cerr << "uri_reference_rule: \"" << text << "\" should match \"" << match << "\"\n";
    }
    BOOST_TEST(it == text.data() + match.size());
}

void the_rule_inside_a_grammar() {
    // The guard's inputs, with a scheme and without: Boost.URL stops a URI at the authority and
    // reads a relative reference's rest as a path, and the rule refuses both alike.
    constexpr std::array<std::string_view, 7> guarded{
        "//a@b@example.com/",        // a second at sign
        "http://a@b@example.com/",   //
        "//example.com:abc/p",       // a port that is not a number
        "http://example.com:abc/p",  //
        "//[::1]x/p",                // something after an IP literal
        "http://[::1]x/p",           //
        "http://h:1%41",             // a percent-encoded octet after the port
    };
    for (const std::string_view text : guarded) {
        invalid_inside_a_grammar(text);
    }
    // A bracket in a query, which Boost.URL's rule would match whole.
    invalid_inside_a_grammar("http://a/?[x] rest");
    invalid_inside_a_grammar("?[x] rest");
    const std::string_view bracket = "http://a/?[x] rest";
    char const* in_uri = bracket.data();
    const auto u = fg::uri_rule.parse(in_uri, bracket.data() + bracket.size());
    BOOST_TEST(u.has_error() && u.error() == urlg::error::invalid);
    BOOST_TEST(in_uri == bracket.data());
    // A zone identifier, which Boost.URL's rule would hold in the IP literal.
    invalid_inside_a_grammar("http://[::1%25x]/ rest");
    invalid_inside_a_grammar("//[::1%25x] rest");
    // A second number sign, which Boost.URL's rule would hold in the fragment.
    invalid_inside_a_grammar("http://a/#x#y rest");
    invalid_inside_a_grammar("#x#y rest");
    const std::string_view sign = "http://a/#x#y rest";
    char const* in_sign = sign.data();
    const auto f = fg::uri_rule.parse(in_sign, sign.data() + sign.size());
    BOOST_TEST(f.has_error() && f.error() == urlg::error::invalid);
    BOOST_TEST(in_sign == sign.data());
    matches_inside_a_grammar("#x%23y rest", "#x%23y");
    const std::string_view after = "http://a@b@example.com/";
    char const* in_after = after.data();
    const auto a = fg::uri_rule.parse(in_after, after.data() + after.size());
    BOOST_TEST(a.has_error() && a.error() == urlg::error::invalid);
    BOOST_TEST(in_after == after.data());
    const std::string_view spaced = "http://[::1]/?q rest";
    char const* in_spaced = spaced.data();
    const auto m = fg::uri_rule.parse(in_spaced, spaced.data() + spaced.size());
    BOOST_TEST(m.has_value() && m.unsafe_value().buffer() == "http://[::1]/?q");
    BOOST_TEST(in_spaced == spaced.data() + 15);
    // What no path segment starts ends a reference as Boost.URL ends it: a colon after the port, a
    // space, and what a query cannot hold.
    matches_inside_a_grammar("http://h:80:x", "http://h:80");
    matches_inside_a_grammar("//h:80:x", "//h:80");
    matches_inside_a_grammar("http://h c", "http://h");
    matches_inside_a_grammar("http://h?q%zz", "http://h?q");
    matches_inside_a_grammar("//h?q%zz", "//h?q");
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
    // An authority that a character outside every path segment ends stays a match.
    const auto authorities =
        urlg::parse("http://u@h:80 //h?q",
                    urlg::tuple_rule(fg::uri_reference_rule, urlg::squelch(urlg::delim_rule(' ')),
                                     fg::uri_reference_rule));
    if (BOOST_TEST(authorities.has_value())) {
        BOOST_TEST_EQ(std::get<0>(*authorities).buffer(), "http://u@h:80");
        BOOST_TEST_EQ(std::get<1>(*authorities).buffer(), "//h?q");
    }
}

}  // namespace

int main() {
    rfc_3986_section_1_1_2();
    uris_and_references();
    the_guard();
    brackets_only_around_an_ip_literal();
    one_number_sign_before_the_fragment();
    boost_url_still_takes_a_number_sign_in_a_fragment();
    boost_url_still_takes_brackets_in_a_query();
    boost_url_still_needs_the_guard();
    no_zone_identifier();
    boost_url_still_takes_a_zone_identifier();
    ipvfuture_keeps_boost_urls_verdict();
    the_rule_inside_a_grammar();
    return boost::report_errors();
}
