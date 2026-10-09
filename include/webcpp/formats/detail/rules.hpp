// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Small terminal rules that Boost.URL's grammar library does not have.

#ifndef WEBCPP_FORMATS_DETAIL_RULES_HPP
#define WEBCPP_FORMATS_DETAIL_RULES_HPP

#include <webcpp/formats/config.hpp>

#include <webcpp/formats/grammar.hpp>

#include <boost/url/grammar/ci_string.hpp>
#include <boost/url/grammar/digit_chars.hpp>
#include <boost/url/grammar/error.hpp>

#include <cstddef>
#include <string_view>

namespace webcpp::formats::detail {

/** A rule that matches exactly `N` characters of `chars`, and yields them. */
template <std::size_t N, class CharSet>
struct exactly_rule_t {
    /** The rule yields the characters it matched. */
    using value_type = grammar::text;

    /** The set every matched character belongs to. */
    CharSet chars;

    /** Matches `N` characters of the set at `it`, or leaves `it` and answers mismatch. */
    grammar::result<grammar::text> parse(char const*& it, char const* end) const noexcept {
        if (static_cast<std::size_t>(end - it) < N) {
            return boost::urls::grammar::error::mismatch;
        }
        for (std::size_t i = 0; i < N; ++i) {
            if (!chars(it[i])) {
                return boost::urls::grammar::error::mismatch;
            }
        }
        char const* const first = it;
        it += N;
        return grammar::text(first, N);
    }
};

/** The rule that matches exactly `N` characters of `chars`. */
template <std::size_t N, class CharSet>
constexpr exactly_rule_t<N, CharSet> exactly(const CharSet& chars) noexcept {
    return exactly_rule_t<N, CharSet>{chars};
}

/** Two ASCII digits: a month, a day, an hour, a minute or a second. */
inline constexpr auto two_digits_rule = exactly<2>(boost::urls::grammar::digit_chars);

/** A rule that matches a literal with its ASCII letters in either case, and yields the text. */
class ci_literal_rule {
public:
    /** The rule yields the text it matched, in the input's case. */
    using value_type = grammar::text;

    /** The rule of `literal`, which outlives the rule (a string literal). */
    constexpr explicit ci_literal_rule(std::string_view literal) noexcept : literal_(literal) {}

    /** Matches the literal at `it`, or leaves `it` and answers mismatch. */
    grammar::result<grammar::text> parse(char const*& it, char const* end) const noexcept {
        if (static_cast<std::size_t>(end - it) < literal_.size()) {
            return boost::urls::grammar::error::mismatch;
        }
        for (std::size_t i = 0; i < literal_.size(); ++i) {
            if (boost::urls::grammar::to_lower(it[i]) !=
                boost::urls::grammar::to_lower(literal_[i])) {
                return boost::urls::grammar::error::mismatch;
            }
        }
        char const* const first = it;
        it += literal_.size();
        return grammar::text(first, literal_.size());
    }

private:
    std::string_view literal_;
};

}  // namespace webcpp::formats::detail

#endif  // WEBCPP_FORMATS_DETAIL_RULES_HPP
