// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Host names: RFC 1123 2.1's labels and lengths at 63/64 and 253/254, A-labels in either case
// judged as idn-hostname judges them, R-LDH labels, and the A-labels of strings IDNA2008 refuses
// that an IDNA library was measured accepting; and internationalized host names, UTS 46's mapping
// and IDNA2008's validity through idna, their lengths, and where their rules stop and why.

#include <webcpp/lexical/hostname.hpp>

#include <webcpp/idna/errors.hpp>
#include <webcpp/idna/options.hpp>
#include <webcpp/idna/uts46.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace lexical = webcpp::lexical;
namespace fg = webcpp::lexical::grammar;
namespace urlg = boost::urls::grammar;

namespace {

void hostname(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(lexical::is_hostname(text), valid)) {
        std::cerr << "hostname: \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(lexical::is_hostname(text), urlg::parse(text, fg::hostname_rule).has_value());
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
    // lexical judges each A-label alone, a decision of its own, so RFC 5893's Bidi rule across
    // the labels of a name is checked only by idn-hostname: this is a host name, and its U-label
    // form, 0a. and HEBREW LETTER ALEF, is no idn-hostname.
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

/** A-labels at a label's 63 octets, in either case, and one octet more refused. */
void a_labels_at_63_and_64_octets() {
    // 55 times a, then LATIN SMALL LETTER A WITH DIAERESIS: an A-label of 63 octets.
    const std::string label63 = "xn--" + std::string(55, 'a') + "-uve";
    BOOST_TEST_EQ(label63.size(), 63U);
    hostname(label63, true);
    hostname("XN--" + std::string(55, 'A') + "-UVE", true);
    hostname(label63 + ".example", true);
    // 56 times a, then the same letter: 64 octets.
    const std::string label64 = "xn--" + std::string(56, 'a') + "-qye";
    BOOST_TEST_EQ(label64.size(), 64U);
    hostname(label64, false);
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

void idn_hostname(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(lexical::is_idn_hostname(text), valid)) {
        std::cerr << "idn-hostname: \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(lexical::is_idn_hostname(text),
                  urlg::parse(text, fg::idn_hostname_rule).has_value());
}

/**
 Names each invalid by a rule of IDNA2008 that an IDNA library was measured missing: DISALLOWED
 exceptions, CONTEXTO, the Bidi rule across labels, a decoded A-label's derived property.
*/
void names_idna2008_refuses() {
    constexpr std::array<std::string_view, 15> refused{
        // U+C2E4, U+302E HANGUL SINGLE DOT TONE MARK, U+B840, then U+D14C U+C2A4 U+D2B8.
        "\xEC\x8B\xA4\xE3\x80\xAE\xEB\xA1\x80.\xED\x85\x8C\xEC\x8A\xA4\xED\x8A\xB8",
        // ARABIC TATWEEL and NKO LAJANYALAN, DISALLOWED exceptions (RFC 5892 2.6).
        "\xD9\x80\xDF\xBA",
        // The vertical kana repeat marks and U+302E, U+302F, U+303B, DISALLOWED exceptions.
        "\xE3\x80\xB1\xE3\x80\xB2\xE3\x80\xB3\xE3\x80\xB4\xE3\x80\xB5\xE3\x80\xAE\xE3\x80\xAF"
        "\xE3\x80\xBB",
        // MIDDLE DOT not between two l (CONTEXTO, RFC 5892 A.3).
        "a\xC2\xB7l",
        "\xC2\xB7l",
        "l\xC2\xB7"
        "a",
        "l\xC2\xB7",
        // GREEK LOWER NUMERAL SIGN before no Greek letter (A.4).
        "\xCE\xB1\xCD\xB5S",
        "\xCE\xB1\xCD\xB5",
        // HEBREW PUNCTUATION GERESH and GERSHAYIM after no Hebrew letter (A.5, A.6).
        "\xD7\xB3\xD7\x91",
        "\xD7\xB4\xD7\x91",
        // KATAKANA MIDDLE DOT in a label of no Hiragana, Katakana or Han (A.9).
        "def\xE3\x83\xBB"
        "abc",
        "\xE3\x83\xBB",
        // A European digit first, then a label of HEBREW LETTER ALEF: the Bidi rule across labels.
        "0a.\xD7\x90",
        // An A-label whose decoded code point IDNA2008 does not allow.
        "xn--7a",
    };
    for (const std::string_view text : refused) {
        idn_hostname(text, false);
    }
}

void idn_host_names() {
    constexpr std::array<std::string_view, 17> valid{
        // U+C2E4 U+B840, then U+D14C U+C2A4 U+D2B8.
        "\xEC\x8B\xA4\xEB\xA1\x80.\xED\x85\x8C\xEC\x8A\xA4\xED\x8A\xB8",
        "b\xC3\xBC"
        "cher.example",
        // An A-label in capital letters, which UTS 46 maps to lowercase.
        "xn--NXASMQ6B",
        // FULLWIDTH DIGIT ONE, TWO and THREE, mapped to ASCII digits.
        "\xEF\xBC\x91\xEF\xBC\x92\xEF\xBC\x93",
        // ZERO WIDTH SPACE, which UTS 46 ignores.
        "a\xE2\x80\x8B"
        "b",
        // MIDDLE DOT between two l.
        "l\xC2\xB7l",
        // GREEK LOWER NUMERAL SIGN before a Greek letter.
        "\xCE\xB1\xCD\xB5\xCE\xB2",
        // HEBREW PUNCTUATION GERESH after a Hebrew letter.
        "\xD7\x90\xD7\xB3\xD7\x91",
        // KATAKANA MIDDLE DOT beside HIRAGANA LETTER SMALL A.
        "\xE3\x83\xBB\xE3\x81\x81",
        // ARABIC-INDIC DIGIT ZERO between two ARABIC LETTER BEH.
        "\xD8\xA8\xD9\xA0\xD8\xA8",
        // IDEOGRAPHIC FULL STOP, FULLWIDTH FULL STOP and HALFWIDTH IDEOGRAPHIC FULL STOP: dots.
        "a\xE3\x80\x82"
        "b",
        "a\xEF\xBC\x8E"
        "b",
        "a\xEF\xBD\xA1"
        "b",
        // FULLWIDTH x and n before "--": an A-label once mapped.
        "\xEF\xBD\x98\xEF\xBD\x8E--nxasmq6b",
        // e and COMBINING ACUTE ACCENT, not in NFC, which UTS 46 normalizes.
        "cafe\xCC\x81.com",
        "hostname",
        "1host",
    };
    for (const std::string_view text : valid) {
        idn_hostname(text, true);
    }
    constexpr std::array<std::string_view, 12> invalid{
        "",
        ".",
        "example.",
        // A trailing IDEOGRAPHIC FULL STOP: an empty root label.
        "example\xE3\x80\x82",
        "a..b",
        "-hello",
        "hello-",
        // An R-LDH label.
        "ab--cd.example",
        // A lead byte without its continuation: ill-formed UTF-8.
        "b\xC3"
        "cher.example",
        // DEVANAGARI KA, ZERO WIDTH JOINER, DEVANAGARI SSA: the joiner after no virama (CONTEXTJ).
        "\xE0\xA4\x95\xE2\x80\x8D\xE0\xA4\xB7",
        // Punycode that does not decode.
        "xn--example-",
        "xn---9uc",
    };
    for (const std::string_view text : invalid) {
        idn_hostname(text, false);
    }
    // R-LDH: a host name (RFC 1123), never an internationalized one (RFC 5890 2.3.1).
    BOOST_TEST(lexical::is_hostname("ab--cd.example"));
    // A host name's labels are judged one by one; an IDN's Bidi rule spans its labels.
    BOOST_TEST(lexical::is_hostname("0a.xn--4db"));
    idn_hostname(std::string_view("b\xC3\xBC"
                                  "cher\0.example",
                                  16),
                 false);
    // The page's side-by-side table.
    idn_hostname("0a.xn--4db", false);
    idn_hostname("xn--nxasmq6b.example", true);
}

void idn_lengths() {
    // "xn--" and 59 octets of Punycode, from 55 "a" and one "u" with diaeresis: 63 octets, the
    // limit; one more "a" makes 64 (Python's punycode codec gives the same lengths).
    idn_hostname(std::string(55, 'a') + "\xC3\xBC", true);
    idn_hostname(std::string(56, 'a') + "\xC3\xBC", false);
    idn_hostname(std::string(64, 'a'), false);
    idn_hostname(std::string(63, 'a') + "." + std::string(63, 'a') + "." + std::string(63, 'a') +
                     "." + std::string(61, 'a'),
                 true);
    idn_hostname(std::string(63, 'a') + "." + std::string(63, 'a') + "." + std::string(63, 'a') +
                     "." + std::string(62, 'a'),
                 false);
}

/** `count` times `text`. */
std::string repeated(std::string_view text, std::size_t count) {
    std::string out;
    for (std::size_t i = 0; i < count; ++i) {
        out += text;
    }
    return out;
}

/** The length of the ASCII form of `name`, converted without the DNS lengths; 0 when refused. */
std::size_t ascii_length(std::string_view name) {
    std::array<char, 1024> ascii{};
    const auto converted =
        webcpp::idna::to_ascii(ascii, name, webcpp::idna::options{.verify_dns_length = false});
    return converted ? converted.value() : 0;
}

/** The DNS lengths count the name's ASCII form, never the octets as written. */
void idn_lengths_are_the_ascii_forms() {
    // FULLWIDTH LATIN SMALL LETTER A, three octets written and one in the ASCII form: labels of 63,
    // 63, 63 and 61 make 753 octets written and 253 in the ASCII form.
    const std::string_view fullwidth_a = "\xEF\xBD\x81";
    const std::string wide = repeated(fullwidth_a, 63) + "." + repeated(fullwidth_a, 63) + "." +
                             repeated(fullwidth_a, 63) + "." + repeated(fullwidth_a, 61);
    BOOST_TEST_EQ(wide.size(), 753U);
    BOOST_TEST_EQ(ascii_length(wide), 253U);
    idn_hostname(wide, true);
    // Three labels of 63, then "u" with diaeresis and 55 "a", whose A-label is 63 octets: 249
    // octets written and 255 in the ASCII form.
    const std::string narrow = std::string(63, 'a') + "." + std::string(63, 'a') + "." +
                               std::string(63, 'a') + ".\xC3\xBC" + std::string(55, 'a');
    BOOST_TEST_EQ(narrow.size(), 249U);
    BOOST_TEST_EQ(ascii_length(narrow), 255U);
    idn_hostname(narrow, false);
    // SOFT HYPHEN, which UTS 46 ignores: 2,002 octets written, "ab" in the ASCII form.
    const std::string ignored = "a" + repeated("\xC2\xAD", 1'000) + "b";
    BOOST_TEST_EQ(ignored.size(), 2'002U);
    BOOST_TEST_EQ(ascii_length(ignored), 2U);
    idn_hostname(ignored, true);
}

/** Matches `text` with `rule`, and checks the error and that the iterator stayed. */
template <class Rule>
void refuses(const Rule& rule, std::string_view text, boost::system::error_code expected) {
    char const* it = text.data();
    const auto r = rule.parse(it, text.data() + text.size());
    if (!BOOST_TEST(r.has_error() && r.error() == expected)) {
        std::cerr << "refused for another reason: \"" << text << "\"\n";
    }
    BOOST_TEST(it == text.data());
}

/** Where the IDN rules stop, and why they refuse: no run, or idna's error, the iterator kept. */
void the_idn_rules_stop_and_report() {
    namespace idna = webcpp::idna;
    const std::string_view text =
        "b\xC3\xBC"
        "cher.example:8080";
    char const* it = text.data();
    const auto host = fg::idn_hostname_rule.parse(it, text.data() + text.size());
    BOOST_TEST(host.has_value() && *host ==
                                       "b\xC3\xBC"
                                       "cher.example");
    char const* at = text.data();
    const auto label = fg::idna_label_rule.parse(at, text.data() + text.size());
    BOOST_TEST(label.has_value() && *label ==
                                        "b\xC3\xBC"
                                        "cher");
    refuses(fg::idn_hostname_rule, ":80", urlg::error::mismatch);
    refuses(fg::idn_hostname_rule, "", urlg::error::mismatch);
    refuses(fg::idna_label_rule, ".example", urlg::error::mismatch);
    refuses(fg::idna_label_rule, "\xE3\x80\x82whole", urlg::error::mismatch);
    refuses(fg::idn_hostname_rule, "a..b:80", idna::error::empty_label);
    refuses(fg::idn_hostname_rule, "-a.example/", idna::error::hyphen_at_start_or_end);
    refuses(fg::idn_hostname_rule, "b\xC3:80", idna::error::invalid_utf8);
    refuses(fg::idna_label_rule, "a\xC2\xB7l.example", idna::error::contexto);
    refuses(fg::idn_hostname_rule, "0a.\xD7\x90", idna::error::bidi);
    // A label of 100,000 octets stops at idna's working storage, rather than be processed.
    std::string hostile(100'000, 'a');
    hostile += "\xC3\xBC";
    refuses(fg::idn_hostname_rule, hostile, idna::error::label_too_long);
    refuses(fg::idna_label_rule, hostile, idna::error::label_too_long);
    // ONE DOT LEADER and SMALL FULL STOP, which IDNA2003 mapped to a dot, are disallowed: only
    // the three separators the label's run stops at map to one.
    refuses(fg::idna_label_rule,
            "a\xE2\x80\xA4"
            "b",
            idna::error::disallowed);
    refuses(fg::idna_label_rule,
            "a\xEF\xB9\x92"
            "b",
            idna::error::disallowed);
}

/** The UTF-8 of `cp`, a scalar value. */
std::string utf8_of(char32_t cp) {
    std::string out;
    if (cp < 0x800U) {
        out += static_cast<char>(0xC0U | (cp >> 6U));
    } else if (cp < 0x10000U) {
        out += static_cast<char>(0xE0U | (cp >> 12U));
        out += static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU));
    } else {
        out += static_cast<char>(0xF0U | (cp >> 18U));
        out += static_cast<char>(0x80U | ((cp >> 12U) & 0x3FU));
        out += static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU));
    }
    out += static_cast<char>(0x80U | (cp & 0x3FU));
    return out;
}

/**
 The three separators a label's run stops at are the only code points UTS 46 maps to a sequence
 holding a dot, in idna's tables: so a label the rule accepts is one label.
*/
void only_the_three_separators_map_to_a_dot() {
    constexpr std::array<char32_t, 3> separators{0x3002, 0xFF0E, 0xFF61};
    std::size_t found = 0;
    for (char32_t cp = 0x80; cp <= 0x10FFFF; ++cp) {
        if (cp >= 0xD800 && cp <= 0xDFFF) {
            continue;
        }
        std::array<char, webcpp::idna::max_domain_length> ascii{};
        const auto converted = webcpp::idna::to_ascii(ascii, "a" + utf8_of(cp) + "b");
        if (!converted ||
            std::string_view(ascii.data(), converted.value()).find('.') == std::string_view::npos) {
            continue;
        }
        ++found;
        if (!BOOST_TEST(std::ranges::find(separators, cp) != separators.end())) {
            std::cerr << "maps to a dot: U+" << std::hex << static_cast<std::uint32_t>(cp)
                      << std::dec << "\n";
        }
    }
    BOOST_TEST_EQ(found, separators.size());
}

}  // namespace

int main() {
    labels_and_names_at_their_limits();
    rfc_1123_labels();
    a_labels_in_either_case();
    a_labels_idna2008_refuses();
    an_a_label_meets_the_bidi_rule_alone();
    a_labels_at_63_and_64_octets();
    the_rule_stops_and_reports();
    a_refusal_leaves_the_iterator();
    names_idna2008_refuses();
    idn_host_names();
    idn_lengths();
    idn_lengths_are_the_ascii_forms();
    the_idn_rules_stop_and_report();
    only_the_three_separators_map_to_a_dot();
    return boost::report_errors();
}
