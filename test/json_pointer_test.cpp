// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// RFC 6901's JSON Pointer and draft-handrews-relative-json-pointer-01's relative one: RFC 6901
// section 5's twelve pointers, every escape, UTF-8 well-formed or refused, NUL as a character,
// and the draft's section 5.1 examples.

#include <webcpp/formats/json_pointer.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>

#include <array>
#include <iostream>
#include <string_view>

namespace formats = webcpp::formats;
namespace fg = webcpp::formats::grammar;
namespace urlg = boost::urls::grammar;

namespace {

void pointer(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(formats::is_json_pointer(text), valid)) {
        std::cerr << "json-pointer: \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(formats::is_json_pointer(text),
                  urlg::parse(text, fg::json_pointer_rule).has_value());
}

void relative(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(formats::is_relative_json_pointer(text), valid)) {
        std::cerr << "relative-json-pointer: \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(formats::is_relative_json_pointer(text),
                  urlg::parse(text, fg::relative_json_pointer_rule).has_value());
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
    constexpr std::array<std::string_view, 11> valid{
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
    };
    for (const std::string_view text : valid) {
        relative(text, true);
    }
    constexpr std::array<std::string_view, 12> invalid{
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
        "0+1",           // an index manipulation, of the draft's later revisions
        "\xD9\xA1/foo",  // ARABIC-INDIC DIGIT ONE, a digit outside ASCII
    };
    for (const std::string_view text : invalid) {
        relative(text, false);
    }
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
    where_the_rules_stop();
    a_view_that_ends_inside_an_escape();
    a_bare_integer_is_a_relative_pointer();
    return boost::report_errors();
}
