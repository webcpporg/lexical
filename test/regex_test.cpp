// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// ECMA-262 patterns in u mode, read by lexical's validator: alternatives and empty patterns,
// assertions, quantifiers and braces, groups with their names, ECMAScript 2025's modifiers and
// duplicate group names, lookarounds of any width, every escape and backreference, classes and
// their ranges, Unicode properties, ill-formed UTF-8 refused and NUL a literal, the Unicode 17
// names the properties are read from, and hostile patterns answered as values: nested up to
// max_regex_nesting and refused past it, wide and long ones judged up to max_regex_length.

#include <webcpp/lexical/detail/regex_tables.hpp>
#include <webcpp/lexical/detail/regex_unicode.hpp>
#include <webcpp/lexical/detail/regex_validator.hpp>
#include <webcpp/lexical/regex.hpp>

#include <boost/core/lightweight_test.hpp>

#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace lexical = webcpp::lexical;
namespace detail = webcpp::lexical::detail;
namespace tables = webcpp::lexical::detail::regex_tables;

namespace {

using detail::regex_verdict;

void regex(std::string_view pattern, bool valid) {
    if (!BOOST_TEST_EQ(lexical::is_regex(pattern), valid)) {
        std::cerr << "regex: \"" << pattern << "\"\n";
    }
}

/** A string of `count` copies of `unit`. */
std::string repeated(std::string_view unit, std::size_t count) {
    std::string text;
    text.reserve(unit.size() * count);
    for (std::size_t i = 0; i < count; ++i) {
        text += unit;
    }
    return text;
}

/** `open` nested `levels` times around `a`, each level closed by `close`. */
std::string nested(std::string_view open, std::string_view close, std::size_t levels) {
    return repeated(open, levels) + "a" + repeated(close, levels);
}

/** The verdict check_regex gives `pattern`, and a why exactly when the verdict is not valid. */
void judged(std::string_view pattern, regex_verdict verdict,
            std::size_t max_nesting = lexical::max_regex_nesting) {
    const detail::regex_check check = detail::check_regex(pattern, max_nesting);
    const bool verdict_held = BOOST_TEST(check.verdict == verdict);
    const bool why_held = BOOST_TEST_EQ(check.why.empty(), verdict == regex_verdict::valid);
    if (!verdict_held || !why_held) {
        std::cerr << "check_regex: \"" << pattern << "\" with nesting " << max_nesting << '\n';
    }
}

void alternatives_and_empty_patterns() {
    regex("", true);
    regex("|", true);
    regex("||", true);
    regex("a|", true);
    regex("|a", true);
    regex("a|b|c", true);
    regex("a||b", true);
    regex("(|)", true);
    regex("(?:)", true);
    regex("()", true);
    regex("a|(b|c)|d", true);
    regex("(?:|a)*", true);
    regex("a)|b", false);
    regex("(a|b", false);
    regex("|*", false);
    regex("a|+", false);
}

void assertions() {
    regex("^", true);
    regex("$", true);
    regex("^$", true);
    regex("\\b", true);
    regex("\\B", true);
    regex(R"(^\b\w+\B$)", true);
    regex("a^b$c", true);
    regex("(^)($)", true);
    regex("^a|b$", true);
    regex("^*", false);
    regex("$+", false);
    regex("\\b*", false);
    regex("\\B{2}", false);
    regex("^{1}", false);
    regex("$?", false);
    regex("a^?", false);
}

void quantifiers_and_braces() {
    regex("a*", true);
    regex("a+", true);
    regex("a?", true);
    regex("a{0}", true);
    regex("a{3}", true);
    regex("a{2,}", true);
    regex("a{2,3}", true);
    regex("a{0,0}", true);
    regex("a*?", true);
    regex("a+?", true);
    regex("a??", true);
    regex("a{2,3}?", true);
    regex("(ab)+", true);
    regex("a{1}b{2,}c*", true);
    regex("a{,5}", false);
    regex("a{2,1}", false);
    regex("a{3,1}", false);
    regex("a{", false);
    regex("a{}", false);
    regex("a{x}", false);
    regex("a{1,2", false);
    regex("a{1 }", false);
    regex("{", false);
    regex("}", false);
    regex("a}", false);
    regex("{1}", false);
    regex("a|{1}", false);
    regex("a**", false);
    regex("a+*", false);
    regex("a???", false);
    regex("a{1}{2}", false);
    regex("a++", false);
    regex("\\{", true);
    regex("\\}", true);
    regex("a\\{1\\}", true);
    regex("a\\{1}", false);
}

void groups_and_names() {
    regex("(a)", true);
    regex("(?:a)", true);
    regex("((a))", true);
    regex("(a)(b)(c)", true);
    regex("(?<n>a)", true);
    regex("(?<$>a)", true);
    regex("(?<_a1>x)", true);
    regex("(?<\\u{61}>x)", true);
    regex("(?<\303\261>x)", true);
    regex(R"((?<a\uD835\uDC9C>x))", true);
    regex("(a", false);
    regex("a)", false);
    regex("(?:a", false);
    regex("(?", false);
    regex("(?x)", false);
    regex("(?<n>", false);
    regex("(?<>a)", false);
    regex("(?<1>a)", false);
    regex("(?<\\u0030a>x)", false);
    regex("(?<a-b>x)", false);
    regex("(?<a b>x)", false);
    regex("(?<a>x", false);
    regex("(?P<n>a)", false);
    regex("(?#comment)", false);
}

void duplicate_group_names() {
    regex("(?<n>a)|(?<n>b)", true);
    regex("(?:(?<n>a)|(?<n>b))\\k<n>", true);
    regex("(?:(?<n>a)|(?<n>b))|(?<n>c)", true);
    regex("(?<n>a)(?<n>b)", false);
    regex("((?<n>a)|b)(?<n>c)", false);
    regex("(?<n>a)|(?:(?<n>b)|c)(?<n>d)", false);
    regex("(?<n>(?<n>a))", false);
    regex("(?<=(?<n>a)|(?<n>b))c", true);
    regex("(?<n>a)|(?<=(?<n>b))", true);
}

void modifiers() {
    regex("(?i:a)", true);
    regex("(?-i:a)", true);
    regex("(?i-:a)", true);
    regex("(?ims-:a)", true);
    regex("(?i-m:a)", true);
    regex("(?m:^a$)", true);
    regex("(?s:.)", true);
    regex("(?i:(?-i:a))", true);
    regex("(?-:a)", false);
    regex("(?ii:a)", false);
    regex("(?i-i:a)", false);
    regex("(?i-s-m:a)", false);
    regex("(?x:a)", false);
    regex("(?i)", false);
    regex("(?i", false);
    regex("(?i:a", false);
}

void lookarounds() {
    regex("(?=a)", true);
    regex("(?!a)", true);
    regex("(?<=a)", true);
    regex("(?<!a)", true);
    regex("a(?=b)c", true);
    regex("(?=(a))\\1", true);
    regex("(?=)", true);
    regex("(?<=)", true);
    regex("(?<!)", true);
    regex("(?<=\\$\\d*)x", true);
    regex("(?<=a+)b", true);
    regex("(?<=a|bc)d", true);
    regex("(?<=(?:ab|c))d", true);
    regex("(?<!x{1,3})y", true);
    regex("(?<=(a))\\1", true);
    regex("(?<=\\1(a))b", true);
    regex("(?<=a*)d", true);
    regex("(?=a)*", false);
    regex("(?!a)+", false);
    regex("(?<=a)?", false);
    regex("(?<!a){2}", false);
    regex("(?<=a)*", false);
    regex("(?=a", false);
    regex("(?<=a", false);
    regex("(?<!a", false);
}

void escapes() {
    regex(R"(\d\D\w\W\s\S)", true);
    regex(R"(\t\n\v\f\r\0)", true);
    regex(R"(\cA\cz)", true);
    regex(R"(\x41\x4a\xFF)", true);
    regex(R"(\x4)", false);
    regex(R"(\xG1)", false);
    regex(R"(\x)", false);
    regex(R"(\u0041\u00e9)", true);
    regex(R"(\u004)", false);
    regex(R"(\u)", false);
    regex(R"(\uD83D\uDE00)", true);
    regex(R"(\uD83D)", true);
    regex(R"(\u{41}\u{1F600})", true);
    regex(R"(\u{})", false);
    regex(R"(\u{41)", false);
    regex(R"(\u{110000})", false);
    regex(R"(\c)", false);
    regex(R"(\c1)", false);
    regex(R"(\c_)", false);
    regex(R"(\0)", true);
    regex(R"(\00)", false);
    regex(R"(\01)", false);
    regex(R"(\8)", false);
    regex(R"(\9)", false);
    regex(R"(a\)", false);
    regex(R"(\^\$\\\.\*\+\?\(\)\[\]\{\}\|\/)", true);
    regex(R"(\-)", false);
    regex(R"(\a)", false);
    regex(R"(\q)", false);
    regex(R"(\_)", false);
    regex(R"(\ )", false);
    regex(R"(\')", false);
    regex(R"(\")", false);
    regex(R"(\:)", false);
    regex(R"(\@)", false);
    regex(R"(\#)", false);
    regex(R"(\<)", false);
    regex(R"(\N)", false);
    regex(R"(\h)", false);
    regex(R"(\e)", false);
    regex(R"(\G)", false);
    regex(R"(\K)", false);
    regex(R"(\i)", false);
    regex(R"(\A)", false);
    regex(R"(abc\Z)", false);
    regex(R"(abc\z)", false);
    regex(R"(\\A)", true);
    regex(R"(\\\A)", false);
    regex(R"([\b])", true);
    regex(R"([\-])", true);
    regex(R"([\^\]\\])", true);
    regex(R"([\0])", true);
    regex(R"([\00])", false);
    regex(R"([\B])", false);
    regex(R"([\k])", false);
    regex(R"([\c])", false);
    regex(R"([\a])", false);
    regex(R"([\1])", false);
    regex(R"([\z])", false);
}

void backreferences() {
    regex("(a)\\1", true);
    regex("\\1(a)", true);
    regex("(a)|\\1", true);
    regex("(a)(b)(c)(d)(e)(f)(g)(h)(i)(j)\\10", true);
    regex("\\1", false);
    regex("(?:a)\\1", false);
    regex("(a)\\2", false);
    regex("(a)\\10", false);
    regex("\\k<n>(?<n>a)", true);
    regex("(?<n>a\\k<n>)", true);
    regex("(?<n>a)\\k<n>", true);
    regex("(?<a>x)\\k<a>(?<b>y)\\k<b>", true);
    regex("\\k<m>(?<n>a)", false);
    regex("\\k<n>", false);
    regex("\\k", false);
    regex("\\k<>", false);
    regex("\\k<n", false);
    regex("(?<n>a)\\k", false);
    regex("(a)\\k<a>", false);
    regex("[\\1]", false);
    regex("[\\k<n>]", false);
}

void classes() {
    regex("[a]", true);
    regex("[^a]", true);
    regex("[]", true);
    regex("[^]", true);
    regex("[a-z]", true);
    regex("[a-z0-9_]", true);
    regex("[-a]", true);
    regex("[a-]", true);
    regex("[a-a]", true);
    regex("[\\u0061-\\u0062]", true);
    regex("[\\x61-b]", true);
    regex("[\\cA-\\cZ]", true);
    regex("[\\b-z]", true);
    regex("[\\-]", true);
    regex(R"([\d\D\s\S\w\W])", true);
    regex(R"([\b\-\]\\])", true);
    regex("[[]", true);
    regex("[(){}|]", true);
    regex("[a&&b]", true);
    regex("[]a]", false);
    regex("[^]a]", false);
    regex("[", false);
    regex("[a", false);
    regex("[^", false);
    regex("[\\]", false);
    regex("]", false);
    regex("[z-a]", false);
    regex("[b-a]", false);
    regex("[\\d-z]", false);
    regex("[a-\\d]", false);
    regex("[\\w-.]", false);
    regex("[\\D-z]", false);
    regex("[\\p{L}-z]", false);
}

void unicode_properties() {
    regex(R"(\p{L}\p{Lu}\p{gc=Lu}\p{General_Category=Uppercase_Letter})", true);
    regex(R"(\p{Script=Greek}\p{sc=Grek}\p{scx=Grek}\p{Script_Extensions=Greek})", true);
    regex(R"(\p{Script=Sidetic}\p{sc=Tai_Yo})", true);
    regex(R"(\p{ASCII}\p{Any}\p{Assigned}\p{ID_Start}\p{IDS}\p{Lower}\p{Emoji})", true);
    regex(R"(\p{digit}\p{cntrl}\p{punct})", true);
    regex(R"(\P{L}[\p{L}\d][^\p{L}a])", true);
    regex(R"([\p{Script=Greek}a])", true);
    regex(R"(\p{Script=L})", false);
    regex(R"(\p{General_Category=Greek})", false);
    regex(R"(\p{sc=Foo})", false);
    regex(R"(\p{Greek})", false);
    regex(R"(\p{Lu=Lu})", false);
    regex(R"(\p{ID_Start=Yes})", false);
    regex(R"(\p{Script_Extensions})", false);
    regex(R"(\p{lu})", false);
    regex(R"(\p{ L})", false);
    regex(R"(\p{L })", false);
    regex(R"(\p{})", false);
    regex(R"(\p{L)", false);
    regex(R"(\p)", false);
    regex(R"(\pL)", false);
    regex(R"(\P{})", false);
    regex(R"(\p{letter})", false);
    regex("\\p{\303\251}", false);
    regex("\\p{" + std::string(200, 'L') + "}", false);
}

void utf8_and_nul() {
    regex("caf\303\251", true);
    regex("\360\237\220\262+", true);
    regex("[\360\237\220\262-\360\237\220\263]", true);
    regex("caf\303", false);
    regex("\377", false);
    regex("\200", false);
    regex("a\300\257", false);
    regex("\340\200\257", false);
    regex("\355\240\200", false);
    regex("\364\220\200\200", false);
    regex(std::string_view("a\0b", 3), true);
    regex(std::string_view("[\0-a]", 5), true);
}

void the_tables() {
    BOOST_TEST_EQ(tables::unicode_version, "17.0.0");
    BOOST_TEST(detail::has_name(tables::general_category_names, "Lu"));
    BOOST_TEST(detail::has_name(tables::general_category_names, "Uppercase_Letter"));
    BOOST_TEST(!detail::has_name(tables::general_category_names, "Greek"));
    BOOST_TEST(detail::has_name(tables::script_names, "Greek"));
    BOOST_TEST(detail::has_name(tables::script_names, "Grek"));
    BOOST_TEST(!detail::has_name(tables::script_names, "Lu"));
    BOOST_TEST(detail::has_name(tables::binary_names, "Emoji"));
    BOOST_TEST(!detail::has_name(tables::binary_names, "Letter"));
    BOOST_TEST(detail::in_ranges(tables::id_start, U'a'));
    BOOST_TEST(!detail::in_ranges(tables::id_start, U'1'));
    BOOST_TEST(detail::in_ranges(tables::id_continue, U'1'));
    BOOST_TEST(detail::is_name_start(U'$'));
    BOOST_TEST(!detail::is_name_start(U'1'));
    BOOST_TEST(detail::is_name_part(U'\u200C'));
    BOOST_TEST(!detail::is_name_part(U'-'));
}

void the_verdicts() {
    judged("", regex_verdict::valid);
    judged("(?<n>a)\\k<n>", regex_verdict::valid);
    judged("a", regex_verdict::valid, 0);
    judged("(a)", regex_verdict::valid, 1);
    judged("(a)", regex_verdict::beyond_bounds, 0);
    judged("((a))", regex_verdict::beyond_bounds, 1);
    judged("((a))", regex_verdict::valid, 2);
    judged(nested("(", ")", lexical::max_regex_nesting), regex_verdict::valid);
    judged(nested("(", ")", lexical::max_regex_nesting + 1), regex_verdict::beyond_bounds);
    // The reader's own limit holds whatever nesting the caller asks for.
    judged(nested("(", ")", 65), regex_verdict::beyond_bounds, 1000);
    judged("a)", regex_verdict::invalid);
    judged("(a", regex_verdict::invalid);
    judged("\\1", regex_verdict::invalid);
    judged("a{3,1}", regex_verdict::invalid);
    judged("\\u{110000}", regex_verdict::invalid);
    judged("\303", regex_verdict::invalid);
    static_assert(detail::check_regex("(?<n>a)\\k<n>", lexical::max_regex_nesting).verdict ==
                      regex_verdict::valid,
                  "check_regex is constexpr");
    static_assert(
        detail::check_regex("a{3,1}", lexical::max_regex_nesting).verdict == regex_verdict::invalid,
        "check_regex is constexpr");
    static_assert(noexcept(lexical::is_regex(std::string_view{})), "is_regex does not throw");
    static_assert(noexcept(detail::check_regex(std::string_view{}, 0)),
                  "check_regex does not throw");
    static_assert(lexical::max_regex_nesting >= 32, "the nesting bound is at least 32");
}

// The patterns past the bounds are valid in V8 and refused here by design: the bounds are the
// library's, so that a hostile pattern cannot exhaust a WebAssembly program's stack or memory.
void hostile_patterns() {
    const std::size_t bound = lexical::max_regex_nesting;
    for (const auto& [open, close] : {
             std::pair<std::string_view, std::string_view>{"(", ")"},
             {"(?:", ")*"},
             {"(?:", ")?"},
             {"(?=", ")"},
             {"(?<=", ")"},
             {"(?<!", ")"},
             {"(?i:", ")"},
             {"(?:a|", ")"},
         }) {
        regex(nested(open, close, bound), true);
        regex(nested(open, close, bound + 1), false);
    }
    regex(repeated("(", 10'000) + "a" + repeated(")", 10'000), false);
    regex(repeated("(?:", 10'000) + "a" + repeated(")*", 10'000), false);
    regex(repeated("(a)", 10'000), true);
    // Alternatives, quantifiers, backreferences and classes are not counted: wide patterns are
    // judged, up to max_regex_length.
    regex("a" + repeated("|a", 5000), true);
    regex(repeated("a*", 5000), true);
    regex("(a)" + repeated("\\1", 5000), true);
    regex("[" + repeated("\\D", 5000) + "]", true);
    // The length: a pattern of max_regex_length bytes is judged, and a longer one refused unread.
    regex(repeated("a", lexical::max_regex_length), true);
    regex(repeated("a", lexical::max_regex_length + 1), false);
    regex(repeated("\\p{L}", lexical::max_regex_length / 5), true);
    regex("[" + repeated("a-z", (lexical::max_regex_length / 3) - 1) + "]", true);
}

}  // namespace

int main() {
    alternatives_and_empty_patterns();
    assertions();
    quantifiers_and_braces();
    groups_and_names();
    duplicate_group_names();
    modifiers();
    lookarounds();
    escapes();
    backreferences();
    classes();
    unicode_properties();
    utf8_and_nul();
    the_tables();
    the_verdicts();
    hostile_patterns();
    return boost::report_errors();
}
