// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The terminals every format shares: strict UTF-8 decoding (RFC 3629), the ASCII character
// sets, RFC 3987's code point classes, and the small rules exactly<N>, ci_literal_rule and
// repeat_rule.

#include <webcpp/lexical/detail/charsets.hpp>
#include <webcpp/lexical/detail/rules.hpp>
#include <webcpp/lexical/detail/utf8.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/url/grammar/delim_rule.hpp>
#include <boost/url/grammar/digit_chars.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/tuple_rule.hpp>

#include <array>
#include <cstddef>
#include <iostream>
#include <string_view>

namespace detail = webcpp::lexical::detail;
namespace urlg = boost::urls::grammar;

namespace {

/** A byte sequence and the code point it decodes to. */
struct decoded {
    std::string_view bytes;
    char32_t cp;
};

void well_formed_sequences_decode() {
    // RFC 3629 section 4's table, at each boundary of each length, and around the surrogates.
    constexpr std::array<decoded, 10> table{
        decoded{.bytes = std::string_view("\x00", 1), .cp = 0x0},
        decoded{.bytes = "\x7F", .cp = 0x7F},
        decoded{.bytes = "\xC2\x80", .cp = 0x80},
        decoded{.bytes = "\xDF\xBF", .cp = 0x7FF},
        decoded{.bytes = "\xE0\xA0\x80", .cp = 0x800},
        decoded{.bytes = "\xED\x9F\xBF", .cp = 0xD7FF},
        decoded{.bytes = "\xEE\x80\x80", .cp = 0xE000},
        decoded{.bytes = "\xEF\xBF\xBF", .cp = 0xFFFF},
        decoded{.bytes = "\xF0\x90\x80\x80", .cp = 0x10000},
        decoded{.bytes = "\xF4\x8F\xBF\xBF", .cp = 0x10FFFF},
    };
    for (const decoded& d : table) {
        std::size_t i = 0;
        char32_t cp = 0;
        BOOST_TEST(detail::decode_utf8(d.bytes, i, cp));
        BOOST_TEST_EQ(static_cast<unsigned long>(cp), static_cast<unsigned long>(d.cp));
        BOOST_TEST_EQ(i, d.bytes.size());
    }
}

void ill_formed_sequences_are_refused() {
    // Each leaves the index and the code point as they were.
    constexpr std::array<std::string_view, 21> refused{
        "\xC0\xAF",          // overlong, of each length
        "\xC1\xBF",          //
        "\xE0\x80\xAF",      //
        "\xF0\x80\x80\xAF",  //
        "\xE0\x9F\xBF",      // overlong by one: U+07FF in three bytes, U+FFFF in four
        "\xF0\x8F\xBF\xBF",  //
        "\xED\xA0\x80",      // the first and the last surrogate
        "\xED\xBF\xBF",      //
        "\xF4\x90\x80\x80",  // above U+10FFFF
        "\xF5\x80\x80\x80",  //
        "\x80",              // a lone continuation byte
        "\xBF",              //
        "\xC3",              // truncated, at each length
        "\xE2\x82",          //
        "\xF0\x9F\x98",      //
        "\xFE",              // bytes no sequence starts with
        "\xFF",              //
        "\xC3\x28",          // a lead byte before an ASCII one
        "\xC3\xC3",          // a byte above 0xBF where a continuation byte must be, at each place
        "\xE2\x82\xE2",      //
        "\xF4\x8F\xBF\xC0",  //
    };
    for (const std::string_view bytes : refused) {
        std::size_t i = 0;
        char32_t cp = U'x';
        if (!BOOST_TEST(!detail::decode_utf8(bytes, i, cp))) {
            std::cerr << "accepted: " << bytes.size() << " bytes\n";
        }
        BOOST_TEST_EQ(i, 0U);
        BOOST_TEST(cp == U'x');
    }
}

void a_sequence_decodes_where_the_index_says() {
    const std::string_view text = "a\xC3\xA9z";
    std::size_t i = 1;
    char32_t cp = 0;
    BOOST_TEST(detail::decode_utf8(text, i, cp));
    BOOST_TEST(cp == U'\u00E9');
    BOOST_TEST_EQ(i, 3U);
    std::size_t past = text.size();
    BOOST_TEST(!detail::decode_utf8(text, past, cp));
}

void a_sequence_ends_with_its_view() {
    // The bytes after the view would complete each sequence: the decoder reads the view alone.
    const std::string_view whole = "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
    for (const std::string_view cut :
         {whole.substr(0, 1), whole.substr(2, 2), whole.substr(5, 3), whole.substr(5, 1)}) {
        std::size_t i = 0;
        char32_t cp = U'x';
        BOOST_TEST(!detail::decode_utf8(cut, i, cp));
        BOOST_TEST_EQ(i, 0U);
        BOOST_TEST(cp == U'x');
    }
}

void ucschar_has_rfc_3987s_ranges() {
    // The first and the last code point of each range.
    constexpr std::array<char32_t, 12> in{
        0xA0,     // U+00A0 to U+D7FF
        0xD7FF,   //
        0xF900,   // U+F900 to U+FDCF
        0xFDCF,   //
        0xFDF0,   // U+FDF0 to U+FFEF
        0xFFEF,   //
        0x10000,  // planes 1 to 13, each but its last two code points
        0x1FFFD,  //
        0x20000,  //
        0xDFFFD,  //
        0xE1000,  // plane 14 from U+E1000, but its last two
        0xEFFFD,  //
    };
    for (const char32_t cp : in) {
        BOOST_TEST(detail::is_ucschar(cp));
    }
    constexpr std::array<char32_t, 16> out{
        0x7F,      // DEL, ASCII's last
        0x9F,      // the last C1 control
        0xD800,    // the first surrogate
        0xE000,    // the private use area, which is iprivate
        0xF8FF,    //
        0xFDD0,    // the noncharacters U+FDD0 to U+FDEF
        0xFDEF,    //
        0xFFF0,    // the specials, U+FFFD among them
        0xFFFD,    //
        0x1FFFE,   // a plane's last two
        0x1FFFF,   //
        0xE0000,   // plane 14 below U+E1000
        0xE0FFF,   //
        0xEFFFE,   // plane 14's last two
        0xF0000,   // the private use planes 15 and 16, which are iprivate
        0x10FFFD,  //
    };
    for (const char32_t cp : out) {
        BOOST_TEST(!detail::is_ucschar(cp));
    }
}

void iprivate_and_bidi_have_their_code_points() {
    constexpr std::array<char32_t, 6> iprivate{
        0xE000,    // the private use area
        0xF8FF,    //
        0xF0000,   // plane 15 but its last two code points
        0xFFFFD,   //
        0x100000,  // plane 16 but its last two code points
        0x10FFFD,  //
    };
    for (const char32_t cp : iprivate) {
        BOOST_TEST(detail::is_iprivate(cp));
    }
    constexpr std::array<char32_t, 5> not_iprivate{0xDFFF, 0xF900, 0xFFFFE, 0x10FFFE, 0xA0};
    for (const char32_t cp : not_iprivate) {
        BOOST_TEST(!detail::is_iprivate(cp));
    }
    constexpr std::array<char32_t, 7> bidi{0x200E, 0x200F, 0x202A, 0x202B, 0x202C, 0x202D, 0x202E};
    for (const char32_t cp : bidi) {
        BOOST_TEST(detail::is_bidi_format(cp));
    }
    constexpr std::array<char32_t, 4> not_bidi{0x200D, 0x2029, 0x202F, 0x2066};
    for (const char32_t cp : not_bidi) {
        BOOST_TEST(!detail::is_bidi_format(cp));
    }
}

void the_host_name_sets_hold_their_grammars() {
    BOOST_TEST(detail::ldh_chars('a') && detail::ldh_chars('Z') && detail::ldh_chars('7'));
    BOOST_TEST(detail::ldh_chars('-') && !detail::ldh_chars('.') && !detail::ldh_chars('_'));
    BOOST_TEST(detail::hostname_chars('.') && !detail::hostname_chars(':'));
}

void the_mail_sets_hold_their_grammars() {
    for (const char c : std::string_view("!#$%&'*+-/=?^_`{|}~aZ0")) {
        BOOST_TEST(detail::atext_chars(c));
    }
    for (const char c : std::string_view("()<>[]:;@\\,.\" ")) {
        BOOST_TEST(!detail::atext_chars(c));
    }
    BOOST_TEST(detail::qtext_smtp_chars(' ') && detail::qtext_smtp_chars('~'));
    BOOST_TEST(!detail::qtext_smtp_chars('"') && !detail::qtext_smtp_chars('\\'));
    BOOST_TEST(!detail::qtext_smtp_chars('\t') && !detail::qtext_smtp_chars('\x7F'));
    BOOST_TEST(detail::quoted_pair_chars(' ') && detail::quoted_pair_chars('"'));
    BOOST_TEST(!detail::quoted_pair_chars('\x1F') && !detail::quoted_pair_chars('\x7F'));
}

void the_template_sets_hold_their_grammars() {
    // RFC 6570's literals with erratum 6937: the apostrophe is one.
    for (const char c : std::string_view("!#$&'()*+,-./09:;=?@AZ[]_az~")) {
        BOOST_TEST(detail::template_literal_chars(c));
    }
    for (const char c : std::string_view(" \"%<>\\^`{|}")) {
        BOOST_TEST(!detail::template_literal_chars(c));
    }
    BOOST_TEST(detail::varchar_chars('_') && !detail::varchar_chars('-') &&
               !detail::varchar_chars('.'));
    for (const char c : std::string_view("+#./;?&=,!@|")) {
        BOOST_TEST(detail::template_operator_chars(c));
    }
    BOOST_TEST(!detail::template_operator_chars('-') && !detail::template_operator_chars('*'));
}

void exactly_matches_its_count_or_leaves_the_input() {
    constexpr auto four = detail::exactly<4>(urlg::digit_chars);
    const std::string_view text = "2026-10";
    char const* it = text.data();
    const auto year = four.parse(it, text.data() + text.size());
    BOOST_TEST(year.has_value() && *year == "2026");
    BOOST_TEST(it == text.data() + 4);
    // The last input ends before a digit of the same text: the rule reads its input alone.
    const std::array<std::string_view, 4> refusals{"202", "20x6", "", std::string_view("2026", 3)};
    for (const std::string_view refused : refusals) {
        char const* at = refused.data();
        const auto r = four.parse(at, refused.data() + refused.size());
        BOOST_TEST(r.has_error() && r.error() == urlg::error::mismatch);
        BOOST_TEST(at == refused.data());
    }
}

void ci_literal_matches_in_any_case() {
    constexpr detail::ci_literal_rule ipv6("IPv6:");
    for (const std::string_view text : {"IPv6:::1", "ipv6:::1", "IPV6:::1"}) {
        char const* it = text.data();
        const auto r = ipv6.parse(it, text.data() + text.size());
        BOOST_TEST(r.has_value() && r->size() == 5);
        BOOST_TEST(it == text.data() + 5);
    }
    const std::array<std::string_view, 4> refusals{
        "IPv4:1",
        "IPv6",
        "IP v6:",
        std::string_view("IPv6:", 4),  // ends before the colon that follows it in memory
    };
    for (const std::string_view text : refusals) {
        char const* it = text.data();
        const auto r = ipv6.parse(it, text.data() + text.size());
        BOOST_TEST(r.has_error() && r.error() == urlg::error::mismatch);
        BOOST_TEST(it == text.data());
    }
}

/** Checks that `rule` matches `match`, a prefix of `text`, and stops after it. */
template <class Rule>
void repeats_to(const Rule& rule, std::string_view text, std::string_view match) {
    char const* it = text.data();
    const auto r = rule.parse(it, text.data() + text.size());
    if (!BOOST_TEST(r.has_value() && r.unsafe_value() == match)) {
        std::cerr << "repeat_rule: \"" << text << "\"\n";
    }
    BOOST_TEST(it == text.data() + match.size());
}

void repeat_matches_from_least_to_most() {
    // A digit, then ",digit" pairs: one to three repetitions in all.
    constexpr auto pair =
        urlg::tuple_rule(urlg::squelch(urlg::delim_rule(',')), urlg::delim_rule(urlg::digit_chars));
    constexpr auto list = detail::repeat_rule(urlg::delim_rule(urlg::digit_chars), pair, 1, 3);
    repeats_to(list, "1", "1");
    repeats_to(list, "1,2x", "1,2");
    repeats_to(list, "1,2,3,4", "1,2,3");  // stops before a fourth
    repeats_to(list, "1,x", "1");          // the broken repetition is given back
    repeats_to(list, "1,", "1");
    constexpr auto any = detail::repeat_rule(urlg::delim_rule(urlg::digit_chars), pair);
    repeats_to(any, "x", "");
    // A first repetition that fails after reading part of its input is given back whole.
    constexpr auto ab = urlg::tuple_rule(urlg::delim_rule('a'), urlg::delim_rule('b'));
    repeats_to(detail::repeat_rule(ab, ab), "ac", "");
    repeats_to(detail::repeat_rule(ab, ab), "ababac", "abab");
    repeats_to(any, "", "");
    for (const std::string_view refused : {"x", "", ",1"}) {
        char const* it = refused.data();
        const auto r = list.parse(it, refused.data() + refused.size());
        BOOST_TEST(r.has_error() && r.error() == urlg::error::mismatch);
        BOOST_TEST(it == refused.data());
    }
}

}  // namespace

int main() {
    well_formed_sequences_decode();
    ill_formed_sequences_are_refused();
    a_sequence_decodes_where_the_index_says();
    a_sequence_ends_with_its_view();
    ucschar_has_rfc_3987s_ranges();
    iprivate_and_bidi_have_their_code_points();
    the_host_name_sets_hold_their_grammars();
    the_mail_sets_hold_their_grammars();
    the_template_sets_hold_their_grammars();
    exactly_matches_its_count_or_leaves_the_input();
    ci_literal_matches_in_any_case();
    repeat_matches_from_least_to_most();
    return boost::report_errors();
}
