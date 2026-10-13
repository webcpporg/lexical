// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// E-mail addresses, RFC 5321 4.1.2's Mailbox: each production and its refusal, the local part at
// 64 and 65 octets, address literals in either case of their tag, a General-address-literal
// refused, and the rule inside a larger grammar; and RFC 6531 3.3's internationalized Mailbox, its
// UTF-8 local part, its domain judged by idna, and its length as written.

#include <webcpp/lexical/email.hpp>

#include <webcpp/idna/errors.hpp>
#include <webcpp/idna/options.hpp>
#include <webcpp/idna/uts46.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>

#include <array>
#include <iostream>
#include <string>
#include <string_view>

namespace lexical = webcpp::lexical;
namespace fg = webcpp::lexical::grammar;
namespace urlg = boost::urls::grammar;

namespace {

void email(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(lexical::is_email(text), valid)) {
        std::cerr << "email: \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(lexical::is_email(text), urlg::parse(text, fg::email_rule).has_value());
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

/** A Mailbox of 254 octets, RFC 5321 4.5.3.1.3's path of 256 less its angle brackets, and of 255.
 */
void mailboxes_at_254_and_255_octets() {
    const std::string label63(63, 'a');
    // 189 octets: two labels of 63 and one of 61, joined by two dots.
    const std::string domain = label63 + "." + label63 + "." + std::string(61, 'a');
    const std::string dot_string = std::string(64, 'a') + "@" + domain;
    BOOST_TEST_EQ(dot_string.size(), 254U);
    email(dot_string, true);
    email(dot_string + "a", false);
    const std::string quoted = "\"" + std::string(62, 'a') + "\"@" + domain;
    BOOST_TEST_EQ(quoted.size(), 254U);
    email(quoted, true);
    email(quoted + "a", false);
    // The longest address literal: a Mailbox with one is at most 117 octets, so never refused for
    // its length.
    const std::string literal =
        std::string(64, 'a') + "@[IPv6:ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255]";
    BOOST_TEST_EQ(literal.size(), 117U);
    email(literal, true);
    // Inside a grammar, the rule refuses the whole Mailbox rather than match a prefix of it.
    const std::string longer = dot_string + "a>rest";
    char const* it = longer.data();
    const auto r = fg::email_rule.parse(it, longer.data() + longer.size());
    BOOST_TEST(r.has_error() && r.error() == urlg::error::invalid);
    BOOST_TEST(it == longer.data());
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
    namespace detail = webcpp::lexical::detail;
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

void idn(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(lexical::is_idn_email(text), valid)) {
        std::cerr << "idn-email: \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(lexical::is_idn_email(text), urlg::parse(text, fg::idn_email_rule).has_value());
}

void idn_mailboxes() {
    constexpr std::array<std::string_view, 11> valid{
        // U+C2E4 U+B840, at, U+C2E4 U+B840, then U+D14C U+C2A4 U+D2B8.
        "\xEC\x8B\xA4\xEB\xA1\x80@\xEC\x8B\xA4\xEB\xA1\x80.\xED\x85\x8C\xEC\x8A\xA4\xED\x8A\xB8",
        // Greek letters in a Dot-string, then in a Quoted-string.
        "\xCE\xB4\xCE\xBF\xCE\xBA\xCE\xB9\xCE\xBC\xCE\xAE@example.com",
        "\"\xCE\xB4\xCE\xBF\xCE\xBA\xCE\xB9\xCE\xBC\xCE\xAE\"@example.com",
        // e and COMBINING ACUTE ACCENT: the domain is normalized, the local part kept as it is.
        "user@cafe\xCC\x81.com",
        "cafe\xCC\x81@example.com",
        // A C1 control, a noncharacter and a code point beyond the BMP: UTF8-non-ascii all.
        "\xC2\x85@example.com",
        "\xEF\xBF\xBF@example.com",
        "\xF0\x9D\x95\x8F@example.com",
        "a@\xC3\xA9.org",
        "a@[IPv6:::1]",
        "joe.bloggs@example.com",
    };
    for (const std::string_view text : valid) {
        idn(text, true);
    }
    constexpr std::array<std::string_view, 13> invalid{
        // FULLWIDTH COMMERCIAL AT is no at sign.
        "user\xEF\xBC\xA0"
        "example.com",
        // A lead byte without its continuation.
        "a\xC3@example.com",
        "\xCE\xB4@-example.com",
        "\xCE\xB4@example.com.",
        "\xCE\xB4@.example.com",
        "\xCE\xB4@example..com",
        "\xCE\xB4@",
        // MIDDLE DOT not between two l (CONTEXTO).
        "a@b\xC2\xB7"
        "c.example",
        // An R-LDH label.
        "a@ab--cd.example",
        // quoted-pairSMTP stays ASCII.
        "\"a\\\xC3\xA9\"@example.com",
        // A quoted-pair of a lead byte alone, and of a continuation byte alone.
        "\"a\\\xC3\"@example.com",
        "\"a\\\xA9\"@example.com",
        "2962",
    };
    for (const std::string_view text : invalid) {
        idn(text, false);
    }
    std::string local32;
    for (int i = 0; i < 32; ++i) {
        local32 += "\xC3\xA9";
    }
    idn(local32 + "@example.com", true);
    idn(local32 + "a@example.com", false);
}

/**
 An internationalized Mailbox of 254 octets as written, in UTF-8, and of 255: the length is the
 written form's, whatever the domain's ASCII form measures.
*/
void idn_mailboxes_at_254_and_255_octets() {
    std::string local;
    for (int i = 0; i < 32; ++i) {
        local += "\xC3\xA9";  // 64 octets
    }
    // Twelve labels of LATIN SMALL LETTER U WITH DIAERESIS, each "xn--tda" in its ASCII form,
    // then ASCII labels: 189 octets as written, 249 in the ASCII form.
    std::string domain;
    for (int i = 0; i < 12; ++i) {
        domain += "\xC3\xBC.";
    }
    domain += std::string(63, 'a') + "." + std::string(63, 'a') + "." + std::string(25, 'a');
    BOOST_TEST_EQ(domain.size(), 189U);
    std::array<char, webcpp::idna::max_domain_length> ascii{};
    const auto converted = webcpp::idna::to_ascii(ascii, domain);
    BOOST_TEST(converted.has_value() && converted.value() == 249U);
    const std::string mailbox = local + "@" + domain;
    BOOST_TEST_EQ(mailbox.size(), 254U);
    // Its ASCII form would be 314 octets.
    idn(mailbox, true);
    BOOST_TEST(lexical::is_idn_hostname(domain + "a"));
    idn(mailbox + "a", false);
    // FULLWIDTH LATIN SMALL LETTER A, three octets as written and one in the ASCII form: 254 and
    // 255 octets as written, 130 and 131 in the ASCII form.
    std::string fullwidth;
    for (int i = 0; i < 62; ++i) {
        fullwidth += "\xEF\xBD\x81";
    }
    const std::string written254 = std::string(64, 'a') + "@" + fullwidth + ".aa";
    BOOST_TEST_EQ(written254.size(), 254U);
    idn(written254, true);
    idn(written254 + "a", false);
    // Inside a grammar, the rule refuses the whole Mailbox rather than match a prefix of it.
    const std::string longer = mailbox + "a>rest";
    char const* it = longer.data();
    const auto r = fg::idn_email_rule.parse(it, longer.data() + longer.size());
    BOOST_TEST(r.has_error() && r.error() == urlg::error::invalid);
    BOOST_TEST(it == longer.data());
}

/** The domain's refusal is idna's own error, and leaves the iterator where it was. */
void an_idn_domain_refused_by_idna() {
    const std::string_view refused =
        "\xCE\xB4@a\xC2\xB7"
        "b.example>rest";
    char const* it = refused.data();
    const auto r = fg::idn_email_rule.parse(it, refused.data() + refused.size());
    BOOST_TEST(r.has_error() && r.error() == webcpp::idna::error::contexto);
    BOOST_TEST(it == refused.data());
    const std::string_view matched =
        "\xCE\xB4@b\xC3\xBC"
        "cher.example>rest";
    char const* at = matched.data();
    const auto m = fg::idn_email_rule.parse(at, matched.data() + matched.size());
    BOOST_TEST(m.has_value() && *m ==
                                    "\xCE\xB4@b\xC3\xBC"
                                    "cher.example");
}

}  // namespace

int main() {
    dot_strings();
    quoted_strings();
    local_parts_at_64_and_65_octets();
    mailboxes_at_254_and_255_octets();
    domains_and_address_literals();
    address_literals_are_boost_urls_addresses();
    a_nul_after_the_domain();
    the_rule_stops_and_reports();
    the_local_part_rule();
    a_refusal_leaves_the_iterator();
    idn_mailboxes();
    idn_mailboxes_at_254_and_255_octets();
    an_idn_domain_refused_by_idna();
    return boost::report_errors();
}
