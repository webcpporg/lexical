// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// ECMA-262 patterns in u mode, through webcpp::regex: SRELL's \A, \Z and \z refused (by
// webcpp::regex), an escaped backslash before them accepted, a leading (?i) refused, ill-formed
// UTF-8 refused, NUL a literal, wide and deep patterns valid up to regex::max_alternatives and
// regex::max_nesting and refused past them (too_complex and too_deep, values, never a trap), and
// every allocation through the resource given.

#include <webcpp/formats/regex.hpp>

#include <webcpp/regex/options.hpp>

#include <boost/core/lightweight_test.hpp>

#include <iostream>
#include <string>
#include <string_view>

#include "counting_resource.hpp"

namespace formats = webcpp::formats;

namespace {

void regex(std::string_view pattern, bool valid) {
    if (!BOOST_TEST_EQ(formats::is_regex(pattern), valid)) {
        std::cerr << "regex: \"" << pattern << "\"\n";
    }
}

void ecma_262_patterns() {
    regex("^[a-z]+$", true);
    regex("\\p{Letter}+", true);
    regex("(?<year>\\d{4})-(?<month>\\d{2})", true);
    regex("(?<=\\$\\d*)x", true);
    regex("^(abc]", false);
    regex("a{,5}", false);
    regex("a++", false);
    regex("[[:alpha:]]", false);
    regex("(?i:a)b", true);
    regex("", true);
}

void srell_extensions_are_refused() {
    regex("\\A", false);
    regex("abc\\Z", false);
    regex("abc\\z", false);
    regex("[\\z]", false);
    regex("\\\\A", true);
    regex(R"(\\\A)", false);
    regex("(?i)abc", false);
}

void utf8_and_nul() {
    regex("caf\xC3\xA9", true);
    regex("\xF0\x9F\x90\xB2+", true);
    regex("caf\xC3", false);
    regex("a\xC0\xAF", false);
    regex("\xED\xA0\x80", false);
    regex(std::string_view("a\0b", 3), true);
}

/**
 10,000 groups side by side compile; wide patterns compile up to regex::max_alternatives and deep
 ones up to regex::max_nesting, and past them each is refused as a value on every target.
*/
void hostile_patterns() {
    std::string side_by_side;
    for (int i = 0; i < 10'000; ++i) {
        side_by_side += "(a)";
    }
    regex(side_by_side, true);
    std::string alternatives = "a";
    for (std::size_t i = 1; i < webcpp::regex::max_alternatives; ++i) {
        alternatives += "|a";
    }
    regex(alternatives, true);
    regex(alternatives + "|a", false);
    const std::size_t bound = webcpp::regex::max_nesting;
    regex(std::string(bound, '(') + "a" + std::string(bound, ')'), true);
    regex(std::string(bound + 1, '(') + "a" + std::string(bound + 1, ')'), false);
    regex(std::string(10'000, '(') + "a" + std::string(10'000, ')'), false);
    std::string quantified;
    for (int i = 0; i < 10'000; ++i) {
        quantified += "(?:";
    }
    quantified += 'a';
    for (int i = 0; i < 10'000; ++i) {
        quantified += ")*";
    }
    regex(quantified, false);
    std::string lookaheads;
    for (int i = 0; i < 10'000; ++i) {
        lookaheads += "(?=";
    }
    lookaheads += 'a';
    lookaheads.append(10'000, ')');
    regex(lookaheads, false);
}

void every_allocation_goes_through_the_resource() {
    webcpp::test::counting_resource resource;
    BOOST_TEST(formats::is_regex("(?<y>\\d{4})-\\p{Letter}+", resource));
    BOOST_TEST_GT(resource.allocations, 0U);
    BOOST_TEST_EQ(resource.deallocations, resource.allocations);
    webcpp::test::counting_resource refusals;
    BOOST_TEST(!formats::is_regex("^(abc]", refusals));
    BOOST_TEST_EQ(refusals.deallocations, refusals.allocations);
    // SRELL makes 19 allocations compiling ^[a-z]+$ through its resource with its match point
    // rewinder off, webcpp::regex's default (23 with it on), and webcpp::regex gives it the
    // resource unchanged: an allocation that escaped the resource would lower the count.
    webcpp::test::counting_resource exact;
    BOOST_TEST(formats::is_regex("^[a-z]+$", exact));
    BOOST_TEST_EQ(exact.allocations, 19U);
}

}  // namespace

int main() {
    ecma_262_patterns();
    srell_extensions_are_refused();
    utf8_and_nul();
    hostile_patterns();
    every_allocation_goes_through_the_resource();
    return boost::report_errors();
}
