// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// RFC 9562 section 4's UUID: the canonical forms in any case, every wrong length and dash
// position, braces and the urn:uuid: prefix refused, and the rule inside a larger grammar.

#include <webcpp/lexical/uuid.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/url/grammar/delim_rule.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/tuple_rule.hpp>

#include <iostream>
#include <string>
#include <string_view>

namespace lexical = webcpp::lexical;
namespace fg = webcpp::lexical::grammar;
namespace urlg = boost::urls::grammar;

namespace {

/** Checks `text` against `valid`, and that the function and the rule agree. */
void check(std::string_view text, bool valid) {
    if (!BOOST_TEST_EQ(lexical::is_uuid(text), valid)) {
        std::cerr << "uuid: \"" << text << "\"\n";
    }
    BOOST_TEST_EQ(lexical::is_uuid(text), urlg::parse(text, fg::uuid_rule).has_value());
}

void canonical_forms_in_any_case() {
    check("2eb8aa08-aa98-11ea-b4aa-73b441d16380", true);
    check("2EB8AA08-AA98-11EA-B4AA-73B441D16380", true);
    check("2eb8aa08-AA98-11ea-B4Aa-73B441D16380", true);
    check("00000000-0000-0000-0000-000000000000", true);
    // The version and variant nibbles are not checked: the suite accepts version 15.
    check("99c17cbb-656f-f64a-940f-1a4568f03487", true);
    // Nor the variant: f is none of RFC 9562's.
    check("2eb8aa08-aa98-11ea-f4aa-73b441d16380", true);
}

void every_wrong_length_and_dash() {
    const std::string canonical = "2eb8aa08-aa98-11ea-b4aa-73b441d16380";
    for (std::size_t i = 0; i < canonical.size(); ++i) {
        std::string shorter = canonical;
        shorter.erase(i, 1);
        check(shorter, false);
        std::string longer = canonical;
        longer.insert(i, 1, canonical[i] == '-' ? '-' : 'a');
        check(longer, false);
    }
    check("2eb8aa08aa9811eab4aa73b441d16380", false);
    check("2eb8aa0-8aa98-11e-ab4aa7-3b441d16380", false);
    check("2eb8aa08-aa98-11ea-b4ga-73b441d16380", false);
}

void every_dash_and_digit_replaced() {
    // The length kept: each dash becomes a digit, and each digit a dash.
    const std::string canonical = "2eb8aa08-aa98-11ea-b4aa-73b441d16380";
    for (std::size_t i = 0; i < canonical.size(); ++i) {
        std::string replaced = canonical;
        replaced[i] = canonical[i] == '-' ? 'a' : '-';
        check(replaced, false);
    }
}

void braces_prefixes_and_nul_are_refused() {
    check("{2eb8aa08-aa98-11ea-b4aa-73b441d16380}", false);
    check("urn:uuid:2eb8aa08-aa98-11ea-b4aa-73b441d16380", false);
    check(std::string_view("2eb8aa08-aa98-11ea-b4aa-73b441d16380\0", 37), false);
    // A NUL in place of a digit of the last group, set by index: no escape can run into a digit.
    std::string nul_inside = "2eb8aa08-aa98-11ea-b4aa-73b441d16380";
    nul_inside[32] = '\0';
    check(nul_inside, false);
}

void the_rule_inside_a_grammar() {
    const auto pair = urlg::parse(
        "2eb8aa08-aa98-11ea-b4aa-73b441d16380/00000000-0000-0000-0000-000000000000",
        urlg::tuple_rule(fg::uuid_rule, urlg::squelch(urlg::delim_rule('/')), fg::uuid_rule));
    BOOST_TEST(pair.has_value());
    const std::string_view broken = "2eb8aa08-aa98";
    char const* it = broken.data();
    const auto r = fg::uuid_rule.parse(it, broken.data() + broken.size());
    BOOST_TEST(r.has_error() && r.error() == urlg::error::invalid);
    BOOST_TEST(it == broken.data());
    const std::string_view other = "x2eb8aa08";
    char const* at = other.data();
    const auto m = fg::uuid_rule.parse(at, other.data() + other.size());
    BOOST_TEST(m.has_error() && m.error() == urlg::error::mismatch);
}

}  // namespace

int main() {
    canonical_forms_in_any_case();
    every_wrong_length_and_dash();
    every_dash_and_digit_replaced();
    braces_prefixes_and_nul_are_refused();
    the_rule_inside_a_grammar();
    return boost::report_errors();
}
