// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Host names: RFC 1123 2.1's labels and lengths at 63/64 and 253/254, A-labels in either case
// judged as idn-hostname judges them, R-LDH labels, and the A-labels of strings IDNA2008 refuses
// that an IDNA library was measured accepting.

#include <webcpp/formats/hostname.hpp>

#include <webcpp/idna/errors.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>

#include <array>
#include <iostream>
#include <string>
#include <string_view>

namespace formats = webcpp::formats;
namespace fg = webcpp::formats::grammar;
namespace urlg = boost::urls::grammar;

namespace {

void hostname(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(formats::is_hostname(text), valid)) {
        std::cerr << "hostname: \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(formats::is_hostname(text), urlg::parse(text, fg::hostname_rule).has_value());
}

void labels_and_names_at_their_limits() {
    const std::string label63(63, 'a');
    hostname(label63 + ".com", true);
    hostname(std::string(64, 'a') + ".com", false);
    // 253 octets: three labels of 63 and one of 61, joined by three dots.
    const std::string name253 =
        label63 + "." + label63 + "." + label63 + "." + std::string(61, 'a');
    BOOST_TEST_EQ(name253.size(), 253U);
    hostname(name253, true);
    hostname(name253 + "a", false);
}

void rfc_1123_labels() {
    constexpr std::array<std::string_view, 8> valid{
        "www.example.com",  //
        "hostname",         //
        "h0stn4me",         //
        "1host",            // a digit first, as RFC 1123 2.1 allows
        "host-name",        //
        "a--b.com",         //
        "ab--cd.example",   // an R-LDH label, not an A-label
        "test.123",         //
    };
    for (const std::string_view text : valid) {
        hostname(text, true);
    }
    constexpr std::array<std::string_view, 10> invalid{
        "",               //
        ".",              //
        ".example",       //
        "example.",       // a trailing dot: an empty last label
        "a..b",           //
        "-hostname",      //
        "hostname-",      //
        "host_name",      //
        "example.com\n",  //
        "example.com ",   //
    };
    for (const std::string_view text : invalid) {
        hostname(text, false);
    }
    // A U-label: a host name is ASCII only.
    hostname(
        "b\xC3\xBC"
        "cher.example",
        false);
    // FULLWIDTH FULL STOP, a dot only after UTS 46's mapping.
    hostname(
        "example\xEF\xBC\x8E"
        "com",
        false);
    hostname(std::string_view("example.com\0", 12), false);
}

void a_labels_in_either_case() {
    hostname("xn--nxasmq6b", true);
    hostname("XN--NXASMQ6B", true);
    hostname("Xn--NxAsMq6B.example", true);
    hostname("xn--9n2bp8q.xn--9t4b11yi5a", true);
    hostname("xn--X", false);
    hostname("XN--aa---o47jg78q", false);
    hostname("xn--hello-txk", false);
    // A host name's labels are judged one by one: RFC 5893's Bidi rule across the labels
    // of a domain is idn-hostname's, so this is a host name and its U-label form, 0a. and
    // HEBREW LETTER ALEF, is no idn-hostname.
    hostname("0a.xn--4db", true);
}

void a_labels_idna2008_refuses() {
    constexpr std::array<std::string_view, 14> refused{
        "xn--07jt112bpxg.xn--9t4b11yi5a",  //
        "xn--chb89f",                      //
        "xn--07jceefgh4c",                 //
        "xn--al-0ea",                      //
        "xn--l-fda",                       //
        "xn--la-0ea",                      //
        "xn--l-gda",                       //
        "xn--S-jib3p",                     //
        "xn--wva3j",                       //
        "xn--5db1e",                       //
        "xn--5db3e",                       //
        "xn--defabc-k64e",                 //
        "xn--vek",                         //
        "xn--7a",                          //
    };
    for (const std::string_view text : refused) {
        hostname(text, false);
    }
}

/** An A-label alone meets RFC 5893's Bidi rule, as a name of one label does. */
void an_a_label_meets_the_bidi_rule_alone() {
    constexpr std::array<std::string_view, 3> refused{
        "xn--a-0hc",  // a, then HEBREW LETTER ALEF: a right-to-left letter after a left-to-right
        "xn--a-zhc",  // HEBREW LETTER ALEF, then a: a left-to-right letter in a right-to-left label
        "xn--1-0hc",  // 1, then HEBREW LETTER ALEF: a label led by a European digit
    };
    for (const std::string_view text : refused) {
        hostname(text, false);
    }
}

void the_rule_stops_and_reports() {
    const std::string_view text = "example.com:8080";
    char const* it = text.data();
    const auto r = fg::hostname_rule.parse(it, text.data() + text.size());
    BOOST_TEST(r.has_value() && *r == "example.com");
    const std::string_view refused = "xn--7a.example";
    char const* at = refused.data();
    const auto e = fg::hostname_rule.parse(at, refused.data() + refused.size());
    // The A-label's refusal is idna's own error.
    BOOST_TEST(e.has_error() && e.error().category() == webcpp::idna::error_category());
    BOOST_TEST(at == refused.data());
    const std::string_view none = ":80";
    char const* from = none.data();
    const auto m = fg::hostname_rule.parse(from, none.data() + none.size());
    BOOST_TEST(m.has_error() && m.error() == urlg::error::mismatch);
}

/** A run that is no host name leaves the iterator where it was, and is invalid, not a mismatch. */
void a_refusal_leaves_the_iterator() {
    constexpr std::array<std::string_view, 3> broken{
        "a..b:80",      // an empty label
        "-a.example/",  // a hyphen first
        "a.b-",         // a hyphen last
    };
    for (const std::string_view text : broken) {
        char const* it = text.data();
        const auto r = fg::hostname_rule.parse(it, text.data() + text.size());
        if (!BOOST_TEST(r.has_error() && r.error() == urlg::error::invalid)) {
            std::cerr << "hostname_rule: \"" << text << "\"\n";
        }
        BOOST_TEST(it == text.data());
    }
}

}  // namespace

int main() {
    labels_and_names_at_their_limits();
    rfc_1123_labels();
    a_labels_in_either_case();
    a_labels_idna2008_refuses();
    an_a_label_meets_the_bidi_rule_alone();
    the_rule_stops_and_reports();
    a_refusal_leaves_the_iterator();
    return boost::report_errors();
}
