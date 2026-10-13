// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// RFC 3987's IRI and IRI reference, mapped to a URI that Boost.URL parses: probes of the corners
// the suite misses, the query and the fragment, one number sign before the fragment, the guard of
// a relative reference, the inline buffer's edge, an ASCII IRI that never touches the resource,
// NUL, and where the rules stop inside a larger grammar.

#include <webcpp/lexical/uri.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/url/grammar/delim_rule.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/tuple_rule.hpp>

#include <array>
#include <iostream>
#include <memory_resource>
#include <string>
#include <string_view>
#include <tuple>

#include "counting_resource.hpp"

namespace lexical = webcpp::lexical;
namespace fg = webcpp::lexical::grammar;
namespace urlg = boost::urls::grammar;

namespace {

/** Checks `text` against `valid`, and that the function and the rule agree. */
void iri(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(lexical::is_iri(text), valid)) {
        std::cerr << "iri: \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(lexical::is_iri(text), urlg::parse(text, fg::iri_rule).has_value());
}

/** Checks `text` against `valid`, and that the function and the rule agree. */
void reference(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(lexical::is_iri_reference(text), valid)) {
        std::cerr << "iri-reference: \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(lexical::is_iri_reference(text),
                  urlg::parse(text, fg::iri_reference_rule).has_value());
}

void corner_probes() {
    iri("http://1.2.3.4.example.com/", true);
    iri("http://example.com:8o/", false);
    iri("http://ex ample.com/", false);
    iri("http://a/\xE2\x80\x8E", false);   // U+200E: RFC 3987 4.1's MUST NOT
    iri("http://a/\xEF\xBF\xBD", false);   // U+FFFD, outside ucschar
    iri("http://a/\xC2\x80", false);       // U+0080, a C1 control
    iri("http://a/\xC2\xA0", true);        // U+00A0, the first ucschar
    iri("http://a/\xEE\x80\x80", false);   // U+E000, iprivate in a path
    iri("http://a/?\xEE\x80\x80", true);   // in a query
    iri("http://a/#\xEE\x80\x80", false);  // in a fragment
    iri("http://[v1.x]/", true);
    iri("http://[V1.x]/", false);  // Boost.URL's verdict (the page's known deviations)
    // A zone identifier, which RFC 3986 holds in no IP literal since RFC 9844.
    iri("http://[::1%25eth0]/", false);
    iri("http://[::1%25%C3%A9]/", false);
    iri("http://\xC3\xA9@[::1%25x]/\xC3\xA9", false);
    reference("//[::1%25eth0]/\xC3\xA9", false);
    iri("http://a/\xC3", false);
    iri("http://a/\xC0\xAF", false);
    iri("http://a/\xED\xA0\x80", false);
    iri("http://\xC6\x92\xC3\xB8\xC3\xB8.\xC3\x9F\xC3\xA5r:/", true);  // an empty port
    iri("HTTP://a", true);
    iri("1http://a", false);
    iri("a:", true);
    iri("http://a:b@c:d@e/", false);
    reference("//a@b@c/", false);
    reference("//a:abc/p", false);
    reference("", true);
    reference("./\xC3\xA9:x", true);
    reference("\xC3\xA9:x", false);
    reference("#\xEE\x80\x80", false);
    reference("a b", false);
}

void non_ascii_only_where_rfc_3986_has_pct_encoded() {
    iri("h\xC3\xA9:x", false);           // a scheme
    iri("http://a:8\xC3\xA9/", false);   // a port
    iri("http://[::\xC3\xA9]/", false);  // an IP literal
    // RFC 3987 adds no ucschar to an IP literal: what would be a zone id is refused as non-ASCII
    // before it is mapped.
    iri("http://[::1%25\xC3\xA9]/", false);
    reference("//[::1%25\xC3\xA9]/", false);
    reference("//u@[::1%25e\xC3\xA9]:80/p", false);
    iri("http://[v1.\xC3\xA9]/", false);  // IPvFuture
    iri("http://\xC3\xA9@[::1]/\xC3\xA9?\xC3\xA9", true);
    iri("http://[::1]/\xC3\xA9", true);
    // A bracket outside an IP literal, which Boost.URL takes in a query.
    iri("http://a/?[\xC3\xA9]", false);
    iri("http://\xC3\xA9/?]", false);
    iri("http://\xC3\xA9/#[x]", false);
    reference("\xC3\xA9?[x]", false);
    reference("\xC3\xA9/[x]", false);
    iri("http://[::1]/\xC3\xA9?%5B\xC3\xA9%5D", true);
    iri("http://\xC3\xA9@\xC3\xA9/\xC3\xA9?\xC3\xA9#\xC3\xA9", true);
    reference("//\xC6\x92\xC3\xB8\xC3\xB8.\xC3\x9F\xC3\xA5r/?\xE2\x88\x82", true);
    iri(std::string_view("http://a/\0", 10), false);
    reference(std::string_view("\xC3\xA9\0", 3), false);
}

void every_bidirectional_formatting_character_is_refused() {
    // U+200E, U+200F and U+202A to U+202E, beside their neighbours, which are ucschar. The
    // literals hold the embeddings and overrides on purpose, each left open: they are the input.
    // NOLINTBEGIN(misc-misleading-bidirectional)
    constexpr std::array<std::string_view, 7> refused{
        "http://a/\xE2\x80\x8E",   // U+200E
        "http://a/\xE2\x80\x8F",   // U+200F
        "http://a/\xE2\x80\xAA",   // U+202A
        "http://a/\xE2\x80\xAB",   // U+202B
        "http://a/\xE2\x80\xAC",   // U+202C
        "http://a/\xE2\x80\xAD",   // U+202D
        "http://a/?\xE2\x80\xAE",  // U+202E, in a query too
    };
    // NOLINTEND(misc-misleading-bidirectional)
    for (const std::string_view text : refused) {
        iri(text, false);
    }
    iri("http://a/\xE2\x80\x8D", true);  // U+200D
    iri("http://a/\xE2\x80\x90", true);  // U+2010
    iri("http://a/\xE2\x80\xA9", true);  // U+2029
    iri("http://a/\xE2\x80\xAF", true);  // U+202F
}

void the_query_is_before_the_first_number_sign() {
    // iprivate is allowed in the query alone: after the first "?" that precedes the first "#".
    iri("http://a/?x?\xEE\x80\x80", true);
    iri("http://a/?\xF3\xB0\x80\x80", true);  // U+F0000, plane 15
    iri("http://a/#?\xEE\x80\x80", false);    // a "?" in the fragment starts no query
    iri("http://a/?\xEE\x80\x80#\xEE\x80\x80", false);
    iri("http://a/?\xEE\x80\x80#x", true);
    iri("http://\xEE\x80\x80/?x", false);  // in a host
    reference("?\xEE\x80\x80", true);
    reference("x#y?\xEE\x80\x80", false);
}

void one_number_sign_before_the_fragment() {
    // RFC 3987's ifragment, as RFC 3986's fragment, holds no "#" but percent-encoded; Boost.URL
    // takes one, and lexical refuses it in the mapped URI.
    iri("http://\xC3\xA9/#x#y", false);
    iri("http://a/#\xC3\xA9#", false);
    iri("http://a/?\xEE\x80\x80#x#\xC3\xA9", false);
    iri("http://a/##", false);
    reference("#\xC3\xA9#", false);
    reference("\xC3\xA9#x#y", false);
    reference("##", false);
    iri("http://a/#\xC3\xA9%23\xC3\xA9", true);
    iri("http://\xC3\xA9/#x?y/%23", true);
    reference("#\xC3\xA9%23", true);
}

void a_relative_reference_keeps_its_guard() {
    // The guard of uri_reference_rule refuses a non-ASCII reference as it refuses an ASCII one.
    reference("//\xC3\xA9:abc/p", false);
    reference("//a@b@\xC3\xA9/", false);
    reference("//\xC3\xA9:80/p", true);
    reference("//\xC3\xA9", true);
    iri("//\xC3\xA9/p", false);  // no scheme
}

void the_inline_buffer_and_the_resource() {
    // "http://a/" is 9 octets, each U+00E9 maps to 6 and "x" to 1: 169 U+00E9 and one "x" map to
    // 1,024.
    std::string at_capacity = "http://a/";
    for (int i = 0; i < 169; ++i) {
        at_capacity += "\xC3\xA9";
    }
    at_capacity += 'x';
    webcpp::test::counting_resource inline_buffer;
    BOOST_TEST(lexical::is_iri(at_capacity, inline_buffer));
    BOOST_TEST_EQ(inline_buffer.allocations, 0U);
    webcpp::test::counting_resource beyond;
    BOOST_TEST(lexical::is_iri(at_capacity + "x", beyond));
    BOOST_TEST_EQ(beyond.allocations, 1U);
    BOOST_TEST_EQ(beyond.bytes, 1025U);
    BOOST_TEST_EQ(beyond.deallocations, 1U);
    // Any IRI of up to 341 octets maps within the buffer: "a:", 168 U+00E9 and one U+20AC are 341.
    std::string worst = "a:";
    for (int i = 0; i < 168; ++i) {
        worst += "\xC3\xA9";
    }
    worst += "\xE2\x82\xAC";
    BOOST_TEST_EQ(worst.size(), 341U);
    webcpp::test::counting_resource none;
    BOOST_TEST(lexical::is_iri_reference(worst, none));
    BOOST_TEST_EQ(none.allocations, 0U);
    // The spec's edge in non-ASCII octets alone: 341 map to 1,023, inline; 342 map to 1,026, one
    // buffer of exactly that length.
    std::string octets341;
    for (int i = 0; i < 113; ++i) {
        octets341 += "\xE2\x82\xAC";
    }
    octets341 += "\xC3\xA9";
    BOOST_TEST_EQ(octets341.size(), 341U);
    webcpp::test::counting_resource stays_inline;
    BOOST_TEST(lexical::is_iri_reference(octets341, stays_inline));
    BOOST_TEST_EQ(stays_inline.allocations, 0U);
    std::string octets342;
    for (int i = 0; i < 171; ++i) {
        octets342 += "\xC3\xA9";
    }
    webcpp::test::counting_resource one_buffer;
    BOOST_TEST(lexical::is_iri_reference(octets342, one_buffer));
    BOOST_TEST_EQ(one_buffer.allocations, 1U);
    BOOST_TEST_EQ(one_buffer.bytes, 1026U);
    BOOST_TEST_EQ(one_buffer.deallocations, 1U);
}

void a_refused_iri_never_touches_the_resource() {
    // The code points are checked before anything is mapped: a long IRI refused for one of them
    // takes no buffer.
    std::string refused = "http://a/";
    for (int i = 0; i < 1'000; ++i) {
        refused += "\xC3\xA9";
    }
    refused += "\xEE\x80\x80";
    BOOST_TEST(!lexical::is_iri(refused, *std::pmr::null_memory_resource()));
    BOOST_TEST(!lexical::is_iri_reference(refused, *std::pmr::null_memory_resource()));
}

void an_ascii_iri_never_touches_the_resource() {
    std::string long_ascii = "http://example.com/";
    long_ascii.append(1'000'000, 'a');
    BOOST_TEST(lexical::is_iri(long_ascii, *std::pmr::null_memory_resource()));
    BOOST_TEST(lexical::is_iri_reference(long_ascii, *std::pmr::null_memory_resource()));
}

void the_rules_inside_a_grammar() {
    // An IRI ends at the first byte no IRI holds, here a space, with non-ASCII text on each side.
    const auto pair =
        urlg::parse("http://a/\xC3\xA9 \xC3\xA9tail",
                    urlg::tuple_rule(fg::iri_rule, urlg::squelch(urlg::delim_rule(' ')),
                                     fg::iri_reference_rule));
    if (BOOST_TEST(pair.has_value())) {
        BOOST_TEST_EQ(std::get<0>(pair.unsafe_value()), "http://a/\xC3\xA9");
        BOOST_TEST_EQ(std::get<1>(pair.unsafe_value()), "\xC3\xA9tail");
    }
    const std::string_view refused = "http://a/\xE2\x80\x8E rest";
    char const* it = refused.data();
    const auto r = fg::iri_rule.parse(it, refused.data() + refused.size());
    BOOST_TEST(r.has_error() && r.error() == urlg::error::invalid);
    BOOST_TEST(it == refused.data());
    const std::string_view none = " x";
    char const* at = none.data();
    const auto m = fg::iri_rule.parse(at, none.data() + none.size());
    BOOST_TEST(m.has_error() && m.error() == urlg::error::mismatch);
    BOOST_TEST(at == none.data());
    // The empty reference is valid: before a byte no IRI holds, the reference rule matches it.
    char const* empty = none.data();
    const auto e = fg::iri_reference_rule.parse(empty, none.data() + none.size());
    BOOST_TEST(e.has_value() && e.unsafe_value().empty());
    BOOST_TEST(empty == none.data());
    // A second number sign, refused whole by both rules.
    const std::string_view sign = "http://a/#\xC3\xA9#x rest";
    char const* in_sign = sign.data();
    const auto s = fg::iri_rule.parse(in_sign, sign.data() + sign.size());
    BOOST_TEST(s.has_error() && s.error() == urlg::error::invalid);
    BOOST_TEST(in_sign == sign.data());
    const std::string_view relative = "#\xC3\xA9#x rest";
    char const* in_relative = relative.data();
    const auto t = fg::iri_reference_rule.parse(in_relative, relative.data() + relative.size());
    BOOST_TEST(t.has_error() && t.error() == urlg::error::invalid);
    BOOST_TEST(in_relative == relative.data());
    // A run refused whole: the rule never matches its valid prefix "http://a/".
    const std::string_view broken = "http://a/%zz rest";
    char const* whole = broken.data();
    const auto w = fg::iri_rule.parse(whole, broken.data() + broken.size());
    BOOST_TEST(w.has_error() && w.error() == urlg::error::invalid);
    BOOST_TEST(whole == broken.data());
}

/** Checks that an IRI followed by `byte` ends before it, inside a larger grammar. */
void ends_before(char byte) {
    const std::string text = std::string("http://a/\xC3\xA9") + byte + "x";
    char const* it = text.data();
    const auto r = fg::iri_rule.parse(it, text.data() + text.size());
    if (!BOOST_TEST(r.has_value() && r.unsafe_value() == "http://a/\xC3\xA9")) {
        std::cerr << "iri_rule does not end before byte " << static_cast<int>(byte) << "\n";
    }
    BOOST_TEST(it == text.data() + 11);
}

void where_an_iri_ends() {
    // RFC 3987's IRI holds no ASCII control, space, double quote, less-than or greater-than sign,
    // backslash, circumflex, grave accent, brace or vertical bar.
    constexpr std::array<char, 14> ends{
        '\0',    // NUL
        '\t',    // a control
        '\n',    //
        '\x1F',  // the last C0 control
        ' ',     //
        '"',     //
        '<',     //
        '>',     //
        '\\',    //
        '^',     //
        '`',     //
        '{',     //
        '|',     //
        '}',     //
    };
    for (const char byte : ends) {
        ends_before(byte);
    }
    ends_before('\x7F');
    // Any other byte belongs to the run, which is then valid or refused whole.
    const std::string_view percent = "http://a/%41b";
    char const* it = percent.data();
    const auto r = fg::iri_rule.parse(it, percent.data() + percent.size());
    BOOST_TEST(r.has_value() && r.unsafe_value() == percent);
    BOOST_TEST(it == percent.data() + percent.size());
    const std::string_view bracket = "http://a/[x]";
    char const* at = bracket.data();
    const auto b = fg::iri_rule.parse(at, bracket.data() + bracket.size());
    BOOST_TEST(b.has_error() && b.error() == urlg::error::invalid);
    BOOST_TEST(at == bracket.data());
}

}  // namespace

int main() {
    corner_probes();
    non_ascii_only_where_rfc_3986_has_pct_encoded();
    every_bidirectional_formatting_character_is_refused();
    the_query_is_before_the_first_number_sign();
    one_number_sign_before_the_fragment();
    a_relative_reference_keeps_its_guard();
    the_inline_buffer_and_the_resource();
    a_refused_iri_never_touches_the_resource();
    an_ascii_iri_never_touches_the_resource();
    the_rules_inside_a_grammar();
    where_an_iri_ends();
    return boost::report_errors();
}
