// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// ECMA-262 patterns in u mode, read by formats' translator and compiled by Boost.Regex: what
// ECMA-262 refuses in u mode refused (Annex B's escapes, Perl's \A, \Z and \z, a leading (?i)),
// ECMAScript 2025's modifiers and duplicate group names, lookbehinds of any width, ill-formed
// UTF-8 refused and NUL a literal, the Unicode 17 tables the classes read, what the translator
// writes, and hostile patterns answered as values on every target: nested up to
// max_regex_nesting and refused past it, wide and long ones judged.

#include <webcpp/formats/regex.hpp>

#include <boost/core/lightweight_test.hpp>

#include <cstddef>
#include <iostream>
#include <memory_resource>
#include <string>
#include <string_view>
#include <utility>

#include "counting_resource.hpp"

namespace formats = webcpp::formats;
namespace detail = webcpp::formats::detail;
namespace tables = webcpp::formats::detail::regex_tables;

namespace {

void regex(std::string_view pattern, bool valid) {
    if (!BOOST_TEST_EQ(formats::is_regex(pattern), valid)) {
        std::cerr << "regex: \"" << pattern << "\"\n";
    }
}

/** What the translator writes for `pattern`, each code point above ASCII as `?`, or its verdict. */
std::string translated(std::string_view pattern) {
    const detail::regex_translation t = detail::translate_regex(
        pattern,
        {.nesting = formats::max_regex_nesting, .alternatives = formats::max_regex_alternatives},
        *std::pmr::get_default_resource());
    if (t.verdict == detail::regex_verdict::invalid) {
        return "invalid";
    }
    if (t.verdict == detail::regex_verdict::beyond_bounds) {
        return "beyond bounds";
    }
    std::string ascii;
    for (const char32_t c : t.pattern) {
        ascii.push_back(c < 0x80 ? static_cast<char>(c) : '?');
    }
    return ascii;
}

void ecma_262_patterns() {
    regex("^[a-z]+$", true);
    regex("\\p{Letter}+", true);
    regex("(?<year>\\d{4})-(?<month>\\d{2})", true);
    regex("^(abc]", false);
    regex("a{,5}", false);
    regex("a++", false);
    regex("a{2,1}", false);
    regex("[[:alpha:]]", false);
    regex("[z-a]", false);
    regex("[\\d-z]", false);
    regex("(?i:a)b", true);
    regex("", true);
    regex("\\1(a)", true);
    regex("(a\\1)", true);
    regex("\\2(a)", false);
    regex("\\k<n>(?<n>a)", true);
    regex("\\k<m>(?<n>a)", false);
    regex("(?<1a>x)", false);
    regex(R"((?<a\u0062>x)\k<ab>)", true);
    regex(R"(\u{1F432}\uD83D\uDC32\x61\u00e9\cJ\0)", true);
    regex("\\u{110000}", false);
    regex("[]", true);
    regex("[^]", true);
    regex("[\\b]\\B", true);
    regex("(?=a)*", false);
    regex("(?<=a)?", false);
    regex("\\b+", false);
}

void annex_b_and_other_dialects_are_refused() {
    regex("\\A", false);
    regex("abc\\Z", false);
    regex("abc\\z", false);
    regex("[\\z]", false);
    regex("\\\\A", true);
    regex(R"(\\\A)", false);
    regex("(?i)abc", false);
    regex("\\a", false);
    regex("\\-", false);
    regex("[\\-]", true);
    regex("\\01", false);
    regex("\\8", false);
    regex("[\\1]", false);
    regex("\\c", false);
    regex("\\x1", false);
    regex("\\u12", false);
    regex("{", false);
    regex("}", false);
    regex("]", false);
    regex("\\pL", false);
    regex("\\p{letter}", false);
    regex("(?P<n>x)", false);
    regex("(?#comment)", false);
    regex("\\q", false);
}

void modifiers() {
    regex("(?i:a)", true);
    regex("(?-i:a)", true);
    regex("(?i-:a)", true);
    regex("(?ims-:a)", true);
    regex("(?i-m:a)", true);
    regex("(?-:a)", false);
    regex("(?ii:a)", false);
    regex("(?i-i:a)", false);
    regex("(?i-s-m:a)", false);
    regex("(?x:a)", false);
    regex("(?i)", false);
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

void lookbehinds_of_any_width() {
    regex("(?<=\\$\\d*)x", true);
    regex("(?<=a+)b", true);
    regex("(?<=a|bc)d", true);
    regex("(?<=(?:ab|c))d", true);
    regex("(?<!x{1,3})y", true);
    regex("(?<=(a))\\1", true);
    regex("(?<=\\1(a))b", true);
    regex("(?<=a*)d", true);
}

void utf8_and_nul() {
    regex("caf\303\251", true);
    regex("\360\237\220\262+", true);
    regex("caf\303", false);
    regex("a\300\257", false);
    regex("\355\240\200", false);
    regex(std::string_view("a\0b", 3), true);
    regex(std::string_view("[\0-a]", 5), true);
}

void unicode_properties() {
    regex(R"(\p{L}\p{Lu}\p{gc=Lu}\p{General_Category=Uppercase_Letter})", true);
    regex(R"(\p{Script=Greek}\p{sc=Grek}\p{scx=Grek}\p{Script_Extensions=Greek})", true);
    regex(R"(\p{Script=Sidetic}\p{sc=Tai_Yo})", true);
    regex(R"(\p{ASCII}\p{Any}\p{Assigned}\p{ID_Start}\p{IDS}\p{Lower})", true);
    regex(R"(\P{L}[\p{L}\d][^\p{L}a])", true);
    regex("\\p{Script=L}", false);
    regex("\\p{General_Category=Greek}", false);
    regex("\\p{sc=Foo}", false);
    regex("\\p{Greek}", false);
    regex("\\p{Lu=Lu}", false);
    regex("\\p{}", false);
    regex("\\p{L", false);
    regex("\\p{\303\251}", false);
    regex("\\p{" + std::string(200, 'L') + "}", false);
}

/** The bit of the class `name` names among `names`, after `first`. */
template <class Names, class Values>
std::size_t bit_of(std::size_t first, std::string_view name, const Names& names,
                   const Values& values) {
    return first + detail::find_name(names, values, name).value_or(9999);
}

/** The tables of Unicode 17.0.0 the classes read, at code points whose properties are known. */
void the_tables() {
    BOOST_TEST_EQ(tables::unicode_version, "17.0.0");
    const std::size_t lu = bit_of(tables::first_general_category, "Lu",
                                  tables::general_category_names, tables::general_category_values);
    const std::size_t letter =
        bit_of(tables::first_general_category, "Letter", tables::general_category_names,
               tables::general_category_values);
    const std::size_t greek =
        bit_of(tables::first_script, "Greek", tables::script_names, tables::script_values);
    const std::size_t greek_extended = bit_of(tables::first_script_extensions, "Grek",
                                              tables::script_names, tables::script_values);
    BOOST_TEST(detail::in_class(lu, U'A'));
    BOOST_TEST(!detail::in_class(lu, U'a'));
    BOOST_TEST(detail::in_class(letter, U'a'));
    BOOST_TEST(detail::in_class(letter, U'\u03C0'));
    BOOST_TEST(!detail::in_class(letter, U'1'));
    BOOST_TEST(detail::in_class(greek, U'\u03A0'));
    // U+0342, COMBINING GREEK PERISPOMENI, is of the script Inherited, and Greek by its
    // extensions.
    BOOST_TEST(!detail::in_class(greek, U'\u0342'));
    BOOST_TEST(detail::in_class(greek_extended, U'\u0342'));
    BOOST_TEST(!detail::in_class(greek_extended, U'a'));
    BOOST_TEST(detail::has_binary(tables::binary::id_start, U'a'));
    BOOST_TEST(!detail::has_binary(tables::binary::id_start, U'1'));
    BOOST_TEST(detail::has_binary(tables::binary::emoji, U'\U0001F432'));
    BOOST_TEST(detail::has_binary(tables::binary::ascii, U'\x7F'));
    BOOST_TEST(!detail::has_binary(tables::binary::ascii, U'\x80'));
    BOOST_TEST(detail::has_binary(tables::binary::any, U'\U0010FFFF'));
    BOOST_TEST(!detail::has_binary(tables::binary::assigned, U'\U0010FFFF'));
    BOOST_TEST(detail::in_class(static_cast<std::size_t>(detail::own_class::space), U'\uFEFF'));
    BOOST_TEST(!detail::in_class(static_cast<std::size_t>(detail::own_class::space), U'\u0085'));
    BOOST_TEST(detail::in_class(static_cast<std::size_t>(detail::own_class::word), U'_'));
    BOOST_TEST(!detail::in_class(static_cast<std::size_t>(detail::own_class::word), U'\u017F'));
    BOOST_TEST(detail::simple_fold(U'K') == U'k');
    BOOST_TEST(detail::simple_fold(U'\u212A') == U'k');
    BOOST_TEST(detail::simple_fold(U'\u017F') == U's');
    BOOST_TEST(detail::simple_fold(U'1') == U'1');
    const detail::case_orbit k = detail::orbit_of(U'k');
    BOOST_TEST_EQ(k.size, 3U);
    BOOST_TEST(k.code_points[0] == U'k');
    BOOST_TEST_EQ(detail::orbit_of(U'1').size, 1U);
}

/** The class a name gives, through Boost's own interface of the traits. */
detail::class_mask class_named(std::u32string_view name) {
    return detail::regex_traits::lookup_classname(name.data(), name.data() + name.size());
}

/** Boost.Regex's traits: the classes Boost names, the translator's #n, and ignoreCase's orbits. */
void the_traits() {
    using traits = detail::regex_traits;
    BOOST_TEST(class_named(U"w") ==
               detail::class_mask::of(static_cast<std::size_t>(detail::own_class::word)));
    BOOST_TEST(class_named(U"#99999") == 0);
    BOOST_TEST(class_named(U"word") == 0);
    // Under ignoreCase \W refuses U+017F, whose orbit holds s, a word character; \w takes it.
    const detail::class_mask word_closed = class_named(U"#0i");
    const detail::class_mask not_word_closed = class_named(U"#0x");
    BOOST_TEST(traits::isctype(U'\u017F', word_closed));
    BOOST_TEST(!traits::isctype(U'\u017F', not_word_closed));
    BOOST_TEST(traits::isctype(U'-', not_word_closed));
    // \P{Ll} under ignoreCase takes a, whose orbit holds A, outside Ll.
    const std::size_t ll = bit_of(tables::first_general_category, "Ll",
                                  tables::general_category_names, tables::general_category_values);
    const std::u32string not_lowercase =
        U"#" + std::u32string(1, static_cast<char32_t>(U'0' + (ll / 10))) +
        std::u32string(1, static_cast<char32_t>(U'0' + (ll % 10))) + U"n";
    BOOST_TEST(traits::isctype(U'a', class_named(not_lowercase)));
    BOOST_TEST(traits::translate_nocase(U'\u212A') == U'k');
    BOOST_TEST_EQ(traits::value(U'f', 16), 15);
    BOOST_TEST_EQ(traits::value(U'f', 10), -1);
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

/** What the translator writes: Boost's perl syntax with ECMA-262's meaning. */
void the_translation() {
    BOOST_TEST_EQ(translated("ab_1"), "ab_1");
    BOOST_TEST_EQ(translated("a.b"), "a[^\\x{A}\\x{D}\\x{2028}\\x{2029}]b");
    BOOST_TEST_EQ(translated("(?s:.)"), "(?:[\\x{0}-\\x{10FFFF}])");
    BOOST_TEST_EQ(translated("^a$"), "\\Aa\\z");
    BOOST_TEST_EQ(translated("(?m:^)"), "(?:(?:\\A|(?<=[\\x{A}\\x{D}\\x{2028}\\x{2029}])))");
    BOOST_TEST_EQ(translated("a{2,}?"), "a{2,}?");
    BOOST_TEST_EQ(translated("\\d\\W"), "[[:#2:]][[:^#0:]]");
    BOOST_TEST_EQ(translated("[\\D\\S]"), "(?:[[:^#2:]]|[[:^#1:]])");
    BOOST_TEST_EQ(translated("[^a-c]"), "[^\\x{61}-\\x{63}]");
    BOOST_TEST_EQ(translated("(a)\\1"), "(a)(?(1)\\g{1})");
    BOOST_TEST_EQ(translated("(?<n>a)|(?<n>b)\\k<n>"), "(a)|(b)(?:(?(1)\\g{1})(?(2)\\g{2}))");
    BOOST_TEST_EQ(translated("(?<=a+b|c)d"), "(?:(?<=ab)|(?<=c))d");
    BOOST_TEST_EQ(translated("(?<=a*)d"), "(?<=)d");
    BOOST_TEST_EQ(translated("\\u{1F432}"), "?");
    BOOST_TEST_EQ(translated("\\("), "\\x{28}");
    BOOST_TEST_EQ(translated("(?i:k)"), "(?i:k)");
    BOOST_TEST_EQ(translated("(?<"), "invalid");
    BOOST_TEST_EQ(translated(std::string(formats::max_regex_nesting + 1, '(')), "beyond bounds");
    BOOST_TEST_EQ(translated(repeated("a?", formats::max_regex_alternatives + 1)), "beyond bounds");
}

/**
 Each kind of group nested max_regex_nesting deep is judged, and one level deeper is refused
 without being judged, a value on every target; side by side, wide and long patterns are judged.
*/
void hostile_patterns() {
    const std::size_t bound = formats::max_regex_nesting;
    for (const auto& [open, close] : {
             std::pair<std::string_view, std::string_view>{"(", ")"},
             {"(?:", ")*"},
             {"(?:", ")?"},
             {"(?=", ")"},
             {"(?<=", ")"},
             {"(?<!", ")"},
             {"(?i:", ")"},
             {"(?:a|", ")"},
             {"(?:(?:a?)?", ")?"},
         }) {
        const bool nests_twice = open == "(?:(?:a?)?";
        const std::size_t levels = nests_twice ? bound - 1 : bound;
        regex(repeated(open, levels) + "a" + repeated(close, levels), true);
        regex(repeated(open, bound + 1) + "a" + repeated(close, bound + 1), false);
    }
    regex(repeated("(", 10'000) + "a" + repeated(")", 10'000), false);
    regex(repeated("(?:", 10'000) + "a" + repeated(")*", 10'000), false);
    regex(repeated("(a)", 10'000), true);
    // The alternatives: each |, quantifier and backreference, up to the bound and one past it.
    const std::size_t wide = formats::max_regex_alternatives;
    regex("a" + repeated("|a", wide), true);
    regex("a" + repeated("|a", wide + 1), false);
    regex(repeated("a*", wide), true);
    regex(repeated("a*", wide + 1), false);
    regex("(a)" + repeated("\\1", wide), true);
    regex("(a)" + repeated("\\1", wide + 1), false);
    regex(repeated("(?<n>a)|", 64) + repeated("\\k<n>", wide / 65), true);
    regex(repeated("(?<n>a)|", 64) + repeated("\\k<n>", (wide / 65) + 1), false);
    regex("[" + repeated("\\D", wide + 1) + "]", true);
    regex("[" + repeated("\\D", wide + 2) + "]", false);
    // The length: a pattern of max_regex_length bytes is judged, and a longer one refused unread.
    regex(repeated("a", formats::max_regex_length), true);
    regex(repeated("a", formats::max_regex_length + 1), false);
    regex(repeated("\\p{L}", formats::max_regex_length / 5), true);
    regex("[" + repeated("a-z", (formats::max_regex_length / 3) - 1) + "]", true);
}

void the_translator_allocates_through_its_resource() {
    webcpp::test::counting_resource resource;
    BOOST_TEST(formats::is_regex("(?<y>\\d{4})-\\p{Letter}+|(?<=\\$\\d*)x", resource));
    BOOST_TEST_GT(resource.allocations, 0U);
    BOOST_TEST_EQ(resource.deallocations, resource.allocations);
    webcpp::test::counting_resource refusals;
    BOOST_TEST(!formats::is_regex("^(abc]", refusals));
    BOOST_TEST_EQ(refusals.deallocations, refusals.allocations);
}

}  // namespace

int main() {
    ecma_262_patterns();
    annex_b_and_other_dialects_are_refused();
    modifiers();
    duplicate_group_names();
    lookbehinds_of_any_width();
    utf8_and_nul();
    unicode_properties();
    the_tables();
    the_traits();
    the_translation();
    hostile_patterns();
    the_translator_allocates_through_its_resource();
    return boost::report_errors();
}
