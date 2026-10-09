// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// E-mail addresses, RFC 5321 4.1.2's Mailbox: each production and its refusal, the local part at
// 64 and 65 octets, address literals in either case of their tag, a General-address-literal
// refused, and the rule inside a larger grammar.

#include <webcpp/formats/email.hpp>

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

void email(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(formats::is_email(text), valid)) {
        std::cerr << "email: \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(formats::is_email(text), urlg::parse(text, fg::email_rule).has_value());
}

void dot_strings() {
    email("joe.bloggs@example.com", true);
    email("te~st@example.com", true);
    email("!#$%&'*+-/=?^_`{|}~@example.com", true);
    email("te.s.t@example.com", true);
    email(".test@example.com", false);
    email("test.@example.com", false);
    email("te..st@example.com", false);
    email("a,b@iana.org", false);
    email("a\tb@iana.org", false);
    email("\x7F@iana.org", false);
    email("a\xC3\xA9@iana.org", false);
    email(std::string_view("a\0@iana.org", 11), false);
}

void quoted_strings() {
    email("\"joe bloggs\"@example.com", true);
    email("\"joe..bloggs\"@example.com", true);
    email("\"joe@bloggs\"@example.com", true);
    email("\"\"@iana.org", true);
    email(R"("\a"@iana.org)", true);
    email(R"("\""@iana.org)", true);
    email(R"("\\"@iana.org)", true);
    email("\"test\\\xC2\xA9\"@iana.org", false);
    email("\"test@iana.org", false);
    email("test\"@iana.org", false);
    email("\"test\"test@iana.org", false);
    email("\"a\tb\"@iana.org", false);
    // UTF8-non-ascii is qtextSMTP only in idn-email (RFC 6531 3.3).
    email("\"\xC3\xA9\"@iana.org", false);
    // NUL is no qtextSMTP.
    email(std::string_view("\"a\0b\"@iana.org", 14), false);
    // quoted-pairSMTP is a backslash and %d32-126: no control after it.
    email("\"a\\\tb\"@iana.org", false);
    email("\"a\\\x7F\"@iana.org", false);
}

void local_parts_at_64_and_65_octets() {
    email(std::string(64, 'a') + "@example.com", true);
    email(std::string(65, 'a') + "@example.com", false);
    email("\"" + std::string(62, 'a') + "\"@example.com", true);
    email("\"" + std::string(63, 'a') + "\"@example.com", false);
}

void domains_and_address_literals() {
    constexpr std::array<std::string_view, 8> valid{
        "joe.bloggs@[127.0.0.1]",     //
        "joe.bloggs@[IPv6:::1]",      //
        "a@[ipv6:::1]",               // the tag in any case
        "a@[IPV6:2001:db8::1]",       //
        "test@255.255.255.255",       // a host name of digits
        "test@io",                    //
        "test@c--n.com",              // an R-LDH label
        "test@xn--nxasmq6b.example",  // an A-label idna accepts
    };
    for (const std::string_view text : valid) {
        email(text, true);
    }
    constexpr std::array<std::string_view, 19> invalid{
        "joe.bloggs@[127.0.0.300]",        //
        "test@[RFC-5322-domain-literal]",  // a General-address-literal
        "a@[]",                            //
        "a@[1.2.3]",                       //
        "a@[1.2.3.4.5]",                   //
        "test@[1.2.3.4",                   //
        "test@a[255.255.255.255]",         //
        "test@-iana.org",                  //
        "test@iana.org.",                  //
        "test@iana..com",                  //
        "a@b@c.org",                       //
        "a@\xC3\xA9.org",                  // a U-label: ASCII only
        "joe.bloggs@",                     //
        "@example.com",                    //
        "test@xn--7a.example",             // an A-label idna refuses
        "a@b(c).org",                      // RFC 5322's comment
        "test@iana.org\n",                 //
        "\"joe\"example.com",              // no at sign after a quoted string
        "joe[127.0.0.1]",                  // no at sign before an address literal
    };
    for (const std::string_view text : invalid) {
        email(text, false);
    }
}

/**
 An address literal's addresses are Boost.URL's IPv4 and IPv6 rules, not RFC 5321 4.1.3's own
 Snum and IPv6-comp.
*/
void address_literals_are_boost_urls_addresses() {
    // RFC 5321's Snum is 1*3DIGIT; RFC 3986's dec-octet refuses a leading zero.
    email("a@[001.2.3.4]", false);
    // RFC 5321's IPv6-comp leaves at least two groups to its "::"; RFC 4291 lets it stand for one.
    email("a@[IPv6:1:2:3:4:5:6::8]", true);
    email("a@[IPv6:1:2:3:4:5:6:7::]", true);
}

/** A NUL after the domain is input the Mailbox leaves over. */
void a_nul_after_the_domain() {
    email(std::string_view("a@iana.org\0", 11), false);
    const std::string_view text("a@iana.org\0", 11);
    char const* it = text.data();
    const auto r = fg::email_rule.parse(it, text.data() + text.size());
    BOOST_TEST(r.has_value() && *r == "a@iana.org");
}

void the_rule_stops_and_reports() {
    const std::string_view text = "a@b.example>rest";
    char const* it = text.data();
    const auto r = fg::email_rule.parse(it, text.data() + text.size());
    BOOST_TEST(r.has_value() && *r == "a@b.example");
    const std::string_view broken = "a@[1.2.3";
    char const* at = broken.data();
    const auto b = fg::email_rule.parse(at, broken.data() + broken.size());
    BOOST_TEST(b.has_error() && b.error() == urlg::error::invalid);
    BOOST_TEST(at == broken.data());
    const std::string_view none = "@example.com";
    char const* from = none.data();
    const auto m = fg::email_rule.parse(from, none.data() + none.size());
    BOOST_TEST(m.has_error() && m.error() == urlg::error::mismatch);
    // A domain's A-label that idna refuses: idna's own error reaches the caller.
    const auto a_label = urlg::parse("test@xn--7a.example", fg::email_rule);
    BOOST_TEST(a_label.has_error() && a_label.error().category() == webcpp::idna::error_category());
}

/** The local part alone: a quoted string that never closes is invalid, not a local part. */
void the_local_part_rule() {
    namespace detail = webcpp::formats::detail;
    constexpr std::array<std::string_view, 3> unclosed{
        "\"",       //
        "\"abc",    //
        R"("a\")",  // the closing quote escaped
    };
    for (const std::string_view text : unclosed) {
        char const* it = text.data();
        const auto r = detail::local_part_rule.parse(it, text.data() + text.size());
        BOOST_TEST(r.has_error() && r.error() == urlg::error::invalid);
        BOOST_TEST(it == text.data());
    }
    const std::string_view quoted = "\"a b\"@";
    char const* it = quoted.data();
    const auto r = detail::local_part_rule.parse(it, quoted.data() + quoted.size());
    BOOST_TEST(r.has_value() && *r == "\"a b\"");
}

/** Each refusal after a local part leaves the iterator where it was, and says why. */
void a_refusal_leaves_the_iterator() {
    constexpr std::array<std::string_view, 5> broken{
        "a@[1.2.3",         // an address literal never closed
        "a@[IPv7:::1]",     // a tag that is not IPv6
        "test\"@iana.org",  // no at sign after the local part
        "joe.bloggs@",      // no domain
        "a@>",              // no domain before a character no domain holds
    };
    for (const std::string_view text : broken) {
        char const* it = text.data();
        const auto r = fg::email_rule.parse(it, text.data() + text.size());
        if (!BOOST_TEST(r.has_error() && r.error() == urlg::error::invalid)) {
            std::cerr << "email_rule: \"" << text << "\"\n";
        }
        BOOST_TEST(it == text.data());
    }
    const std::string_view refused = "test@xn--7a.example";
    char const* at = refused.data();
    const auto e = fg::email_rule.parse(at, refused.data() + refused.size());
    BOOST_TEST(e.has_error() && e.error().category() == webcpp::idna::error_category());
    BOOST_TEST(at == refused.data());
}

}  // namespace

int main() {
    dot_strings();
    quoted_strings();
    local_parts_at_64_and_65_octets();
    domains_and_address_literals();
    address_literals_are_boost_urls_addresses();
    a_nul_after_the_domain();
    the_rule_stops_and_reports();
    the_local_part_rule();
    a_refusal_leaves_the_iterator();
    return boost::report_errors();
}
