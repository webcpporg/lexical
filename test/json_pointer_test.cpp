// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// RFC 6901's JSON Pointer and the relative one of draft-handrews-relative-json-pointer-01 and
// draft-bhutton-relative-json-pointer-00: RFC 6901 section 5's twelve pointers, every escape,
// UTF-8 well-formed or refused, NUL as a character, the drafts' section 5.1 examples, and the
// index manipulation under each policy.

#include <webcpp/lexical/json_pointer.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>

#include <array>
#include <iostream>
#include <string_view>

namespace lexical = webcpp::lexical;
namespace fg = webcpp::lexical::grammar;
namespace urlg = boost::urls::grammar;

namespace {

void pointer(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(lexical::is_json_pointer(text), valid)) {
        std::cerr << "json-pointer: \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(lexical::is_json_pointer(text),
                  urlg::parse(text, fg::json_pointer_rule).has_value());
}

void relative(std::string_view text, bool valid, lexical::index_manipulation manipulation) {
    if (!BOOST_TEST_EQ(lexical::is_relative_json_pointer(text, manipulation), valid)) {
        std::cerr << "relative-json-pointer, index manipulation "
                  << (manipulation == lexical::index_manipulation::allowed ? "allowed" : "refused")
                  << ": \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(lexical::is_relative_json_pointer(text, manipulation),
                  urlg::parse(text, fg::relative_json_pointer_rule_t{manipulation}).has_value());
}

/** Checks `text` under both policies, the default included. */
void relative(std::string_view text, bool valid) {
    relative(text, valid, lexical::index_manipulation::allowed);
    relative(text, valid, lexical::index_manipulation::refused);
    BOOST_TEST_EQ(lexical::is_relative_json_pointer(text),
                  lexical::is_relative_json_pointer(text, lexical::index_manipulation::allowed));
}

void rfc_6901_section_5() {
    // Each with the value it points to in the section's document.
    constexpr std::array<std::string_view, 12> pointers{
        "",        // the whole document
        "/foo",    // ["bar", "baz"]
        "/foo/0",  // "bar"
        "/",       // 0
        "/a~1b",   // 1
        "/c%d",    // 2
        "/e^f",    // 3
        "/g|h",    // 4
        "/i\\j",   // 5
        "/k\"l",   // 6
        "/ ",      // 7
        "/m~0n",   // 8
    };
    for (const std::string_view text : pointers) {
        pointer(text, true);
    }
}

void escapes() {
    pointer("/~0~1~0~1", true);
    pointer("/~01", true);
    pointer("/~2", false);
    pointer("/~", false);
    pointer("/foo~", false);
    pointer("/~a", false);
    pointer("foo", false);
    pointer("#/foo", false);
    pointer("/foo/-", true);
    pointer("/~1.1", true);
}

void utf8_and_nul() {
    pointer("/\xC3\xA9t\xC3\xA9", true);
    pointer("/\xF0\x9F\x98\x80", true);
    pointer("/\xC3", false);
    pointer("/\xC0\xAF", false);
    pointer("/\xED\xA0\x80", false);
    pointer("/a/\xFF", false);
    // RFC 6901's unescaped holds %x00: NUL is a character of a reference token.
    pointer(std::string_view("/a\0b", 4), true);
}

void relative_pointers_of_section_5_1() {
    constexpr std::array<std::string_view, 12> valid{
        "0",                        // the section's examples, from "baz"
        "1/0",                      //
        "2/highly/nested/objects",  //
        "0#",                       //
        "1#",                       //
        "0/objects",                // and from {"objects": true}
        "1/nested/objects",         //
        "2/foo/0",                  //
        "120/foo/bar",              // an integer of several digits
        "0//",                      // two empty reference tokens
        "99#",                      // nine, the last digit, first and after it
        "100",                      // a zero after the first digit is a digit
    };
    for (const std::string_view text : valid) {
        relative(text, true);
    }
    constexpr std::array<std::string_view, 11> invalid{
        "",              // no integer
        "/foo",          //
        "-1/foo",        // a sign
        "+1/foo",        //
        "01/a",          // a leading zero
        "01#",           //
        "0##",           // something after the "#"
        "1#/foo",        //
        "0/~2",          // a json-pointer that breaks a rule
        "1foo",          // neither "#" nor "/" after the integer
        "\xD9\xA1/foo",  // ARABIC-INDIC DIGIT ONE, a digit outside ASCII
    };
    for (const std::string_view text : invalid) {
        relative(text, false);
    }
}

void index_manipulation_under_each_policy() {
    // draft-bhutton-relative-json-pointer-00's index manipulation, a sign and a non-negative
    // integer after the integer, before a json-pointer or "#": valid when allowed (2020-12 and v1),
    // never when refused (7 and 2019-09).
    constexpr std::array<std::string_view, 10> manipulated{
        "0-1",      // the draft's examples, from "baz"
        "0-1#",     //
        "0+1/foo",  // before a json-pointer
        "1-1#",     // and before "#"
        "1+2#",     //
        "0+0",      // zero, by the draft's ABNF
        "0-0",      //
        "0-0#",     //
        "2+10/a",   // an integer of several digits, a zero among them
        "0+9",      // nine, the last digit
    };
    for (const std::string_view text : manipulated) {
        relative(text, true, lexical::index_manipulation::allowed);
        relative(text, false, lexical::index_manipulation::refused);
        // Allowed is the default, of the function and of the rule object.
        BOOST_TEST(lexical::is_relative_json_pointer(text));
        BOOST_TEST(urlg::parse(text, fg::relative_json_pointer_rule).has_value());
    }
    constexpr std::array<std::string_view, 8> broken{
        "0+01",    // a leading zero after the sign
        "0-01#",   //
        "0+",      // a sign without digits
        "0-/foo",  //
        "0+#",     // a sign followed by "#"
        "0-#",     //
        "0+-1",    // two signs
        "0+1-1",   // two manipulations
    };
    for (const std::string_view text : broken) {
        relative(text, false);
    }
}

void where_the_manipulation_stops_the_rule() {
    // Allowed, the manipulation is part of the pointer; refused, or broken, the rule stops after
    // the integer, where the sign is, as an empty json-pointer may end it there.
    const std::string_view text = "0+1/a";
    char const* it = text.data();
    const auto whole = fg::relative_json_pointer_rule.parse(it, text.data() + text.size());
    BOOST_TEST(whole.has_value() && *whole == "0+1/a");
    char const* at = text.data();
    const auto refused =
        fg::relative_json_pointer_rule_t{lexical::index_manipulation::refused}.parse(
            at, text.data() + text.size());
    BOOST_TEST(refused.has_value() && *refused == "0");
    BOOST_TEST(at == text.data() + 1);
    const std::string_view sign = "0+#";
    char const* from = sign.data();
    const auto bare = fg::relative_json_pointer_rule.parse(from, sign.data() + sign.size());
    BOOST_TEST(bare.has_value() && *bare == "0");
    BOOST_TEST(from == sign.data() + 1);
}

void where_the_rules_stop() {
    // A JSON Pointer holds every code point but its own separators, so inside a larger grammar
    // it runs to the end; a relative pointer's "#" ends it.
    const std::string_view text = "2#rest";
    char const* it = text.data();
    const auto r = fg::relative_json_pointer_rule.parse(it, text.data() + text.size());
    BOOST_TEST(r.has_value() && *r == "2#");
    const std::string_view broken = "/a~2";
    char const* at = broken.data();
    const auto p = fg::json_pointer_rule.parse(at, broken.data() + broken.size());
    BOOST_TEST(p.has_error() && p.error() == urlg::error::invalid);
    BOOST_TEST(at == broken.data());
    const std::string_view none = "x";
    char const* from = none.data();
    const auto m = fg::relative_json_pointer_rule.parse(from, none.data() + none.size());
    BOOST_TEST(m.has_error() && m.error() == urlg::error::mismatch);
}

void what_the_rules_leave_where_they_fail_or_match_nothing() {
    // The json-pointer rule matches empty text where no "/" starts the input, and never answers
    // mismatch; the relative rule leaves its iterator where it was when its pointer breaks.
    const std::string_view word = "foo";
    char const* it = word.data();
    const auto empty = fg::json_pointer_rule.parse(it, word.data() + word.size());
    BOOST_TEST(empty.has_value() && empty->empty());
    BOOST_TEST(it == word.data());
    const std::string_view broken = "0/~2";
    char const* at = broken.data();
    const auto r = fg::relative_json_pointer_rule.parse(at, broken.data() + broken.size());
    BOOST_TEST(r.has_error() && r.error() == urlg::error::invalid);
    BOOST_TEST(at == broken.data());
}

void the_scanner_refuses_what_no_digit_starts() {
    // The rule checks the first digit itself, to answer mismatch; the scanner guards it again.
    constexpr auto allowed = lexical::index_manipulation::allowed;
    BOOST_TEST(!lexical::detail::relative_json_pointer_length("", allowed).has_value());
    BOOST_TEST(!lexical::detail::relative_json_pointer_length("x", allowed).has_value());
    BOOST_TEST(!lexical::detail::relative_json_pointer_length("/0", allowed).has_value());
}

void a_view_that_ends_inside_an_escape() {
    // The byte after the view, in memory, would complete the escape: the rule reads its input
    // alone, and refuses the "~" the view ends with.
    const std::string_view cut("/~0", 2);
    pointer(cut, false);
    char const* it = cut.data();
    const auto r = fg::json_pointer_rule.parse(it, cut.data() + cut.size());
    BOOST_TEST(r.has_error() && r.error() == urlg::error::invalid);
    BOOST_TEST(it == cut.data());
}

void a_bare_integer_is_a_relative_pointer() {
    // An empty json-pointer may follow the integer, so the rule stops after it, at whatever is
    // neither "#" nor "/"; "0" is the whole integer before another digit.
    const std::string_view text = "12x";
    char const* it = text.data();
    const auto r = fg::relative_json_pointer_rule.parse(it, text.data() + text.size());
    BOOST_TEST(r.has_value() && *r == "12");
    BOOST_TEST(it == text.data() + 2);
    const std::string_view zero = "01#";
    char const* at = zero.data();
    const auto z = fg::relative_json_pointer_rule.parse(at, zero.data() + zero.size());
    BOOST_TEST(z.has_value() && *z == "0");
}

}  // namespace

int main() {
    rfc_6901_section_5();
    escapes();
    utf8_and_nul();
    relative_pointers_of_section_5_1();
    index_manipulation_under_each_policy();
    where_the_manipulation_stops_the_rule();
    where_the_rules_stop();
    what_the_rules_leave_where_they_fail_or_match_nothing();
    the_scanner_refuses_what_no_digit_starts();
    a_view_that_ends_inside_an_escape();
    a_bare_integer_is_a_relative_pointer();
    return boost::report_errors();
}
