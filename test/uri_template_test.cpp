// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// RFC 6570's URI Template with erratum 6937: section 1.2's templates of levels 1 to 4, the 22
// probes of the corners the suite misses, literals, names and prefixes at their edges, NUL, and
// where the rule stops.

#include <webcpp/lexical/uri_template.hpp>

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

/** Checks `text` against `valid`, and that the function and the rule agree. */
void uri_template(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(lexical::is_uri_template(text), valid)) {
        std::cerr << "uri-template: \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(lexical::is_uri_template(text),
                  urlg::parse(text, fg::uri_template_rule).has_value());
}

void rfc_6570_section_1_2() {
    constexpr std::array<std::string_view, 66> templates{
        "{var}",             // level 1
        "{hello}",           //
        "{+var}",            // level 2
        "{+hello}",          //
        "{+path}/here",      //
        "here?ref={+path}",  //
        "X{#var}",           //
        "X{#hello}",         //
        "map?{x,y}",         // level 3
        "{x,hello,y}",       //
        "{+x,hello,y}",      //
        "{+path,x}/here",    //
        "{#x,hello,y}",      //
        "{#path,x}/here",    //
        "X{.var}",           //
        "X{.x,y}",           //
        "{/var}",            //
        "{/var,x}/here",     //
        "{;x,y}",            //
        "{;x,y,empty}",      //
        "{?x,y}",            //
        "{?x,y,empty}",      //
        "?fixed=yes{&x}",    //
        "{&x,y,empty}",      //
        "{var:3}",           // level 4
        "{var:30}",          //
        "{list}",            //
        "{list*}",           //
        "{keys}",            //
        "{keys*}",           //
        "{+path:6}/here",    //
        "{+list}",           //
        "{+list*}",          //
        "{+keys}",           //
        "{+keys*}",          //
        "{#path:6}/here",    //
        "{#list}",           //
        "{#list*}",          //
        "{#keys}",           //
        "{#keys*}",          //
        "X{.var:3}",         //
        "X{.list}",          //
        "X{.list*}",         //
        "X{.keys}",          //
        "X{.keys*}",         //
        "{/var:1,var}",      //
        "{/list}",           //
        "{/list*}",          //
        "{/list*,path:4}",   //
        "{/keys}",           //
        "{/keys*}",          //
        "{;hello:5}",        //
        "{;list}",           //
        "{;list*}",          //
        "{;keys}",           //
        "{;keys*}",          //
        "{?var:3}",          //
        "{?list}",           //
        "{?list*}",          //
        "{?keys}",           //
        "{?keys*}",          //
        "{&var:3}",          //
        "{&list}",           //
        "{&list*}",          //
        "{&keys}",           //
        "{&keys*}",          //
    };
    for (const std::string_view text : templates) {
        uri_template(text, true);
    }
}

void corner_probes() {
    uri_template("{", false);
    uri_template("}", false);
    uri_template("{{var}}", false);
    uri_template("{v:9999}", true);
    uri_template("{v*:3}", false);
    uri_template("{a b}", false);
    uri_template("{_a}", true);
    uri_template("{a.}", false);
    uri_template("{..a}", false);
    uri_template("{-var}", false);
    uri_template("{\xC3\xA4}", false);
    uri_template("a<b", false);
    uri_template("a\"b", false);
    uri_template("a^b", false);
    uri_template("a|b", false);
    uri_template("a\\b", false);
    uri_template("a`b", false);
    uri_template(
        "a\xC2\x80"
        "b",
        false);
    uri_template(
        "a\xEF\xBF\xBD"
        "b",
        false);
    uri_template("a\xC3", false);
    uri_template("{v:5}{w*}/\xC3\xA9{?x,y}", true);
    uri_template("{v:}", false);
}

void literals_and_names() {
    uri_template("", true);
    uri_template("a'b", true);  // erratum 6937
    uri_template("%20{v}%2f", true);
    uri_template("a%", false);
    uri_template("a%2", false);
    uri_template("a%GG", false);
    uri_template("a b", false);
    uri_template("a%%41", false);
    uri_template("{%41b}", true);
    uri_template("{a%2}", false);
    uri_template("{%}", false);
    uri_template("{}", false);
    uri_template("{,}", false);
    uri_template("{+}", false);
    uri_template("{v,}", false);
    uri_template("{,v}", true);  // "," is an operator of op-reserve
    uri_template("{,+var}", false);
    uri_template("{=v}{!v}{@v}{|v}", true);
    uri_template("{v:10000}", false);
    uri_template("{v:1000}", true);
    uri_template("{v:0}", false);
    uri_template("{v:01}", false);
    uri_template("{v*}", true);
    uri_template("{v**}", false);
    uri_template("{a,}", false);
    uri_template("{a..b}", false);
    uri_template("{a.b.c}", true);
    uri_template("{a.%41}", true);
    uri_template("/\xEE\x80\x80/\xF3\xB0\x80\x80", true);  // iprivate, BMP and plane 15
    uri_template("/\xC2\xA0/\xF0\x90\x80\x80", true);      // ucschar, BMP and plane 1
    uri_template("/\xEF\xB7\x90", false);                  // U+FDD0, a noncharacter
    uri_template("/\xED\xA0\x80", false);                  // a surrogate
    uri_template("/\xC0\xAF", false);                      // an overlong "/"
    uri_template(std::string_view("a\0b", 3), false);
    uri_template(std::string_view("{a\0}", 4), false);
}

/** Checks that the rule refuses `text` as invalid, its iterator unchanged. */
void invalid_inside_a_grammar(std::string_view text) {
    char const* it = text.data();
    const auto r = fg::uri_template_rule.parse(it, text.data() + text.size());
    if (!BOOST_TEST(r.has_error() && r.error() == urlg::error::invalid)) {
        std::cerr << "uri_template_rule: \"" << text << "\"\n";
    }
    BOOST_TEST(it == text.data());
}

/** Checks that the rule matches `match`, a prefix of `text`, and stops after it. */
void matches_inside_a_grammar(std::string_view text, std::string_view match) {
    char const* it = text.data();
    const auto r = fg::uri_template_rule.parse(it, text.data() + text.size());
    if (!BOOST_TEST(r.has_value() && r.unsafe_value() == match)) {
        std::cerr << "uri_template_rule: \"" << text << "\"\n";
    }
    BOOST_TEST(it == text.data() + match.size());
}

void where_the_rule_stops() {
    matches_inside_a_grammar("{a}/b }rest", "{a}/b");
    matches_inside_a_grammar("x\xC3\xA9 y", "x\xC3\xA9");
    matches_inside_a_grammar("x\xC3", "x");
    matches_inside_a_grammar(" x", "");
    // An expression or a pct-encoded triplet that starts and breaks makes the rule fail whole.
    invalid_inside_a_grammar("x{a");
    invalid_inside_a_grammar("x{a b}");
    invalid_inside_a_grammar("{");
    invalid_inside_a_grammar("x%G");
    invalid_inside_a_grammar("x{a}%");
}

}  // namespace

int main() {
    rfc_6570_section_1_2();
    corner_probes();
    literals_and_names();
    where_the_rule_stops();
    return boost::report_errors();
}
