// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// RFC 3339's dates, times and durations: its examples, every month's length, the Gregorian leap
// years, leap seconds at each offset under both policies, the case of T and Z, the fraction's
// length, appendix A's durations, and where each rule stops inside a larger grammar.

#include <webcpp/lexical/date_time.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/url/grammar/delim_rule.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/tuple_rule.hpp>

#include <array>
#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <tuple>

namespace lexical = webcpp::lexical;
namespace fg = webcpp::lexical::grammar;
namespace urlg = boost::urls::grammar;

namespace {

/** A string and whether the format holds it. */
struct sample {
    std::string_view text;
    bool valid;
};

/** Checks each sample with `is`, and that `is` and `rule` agree on it. */
template <class Is, class Rule>
void check(std::string_view name, std::span<const sample> samples, Is is, const Rule& rule) {
    for (const sample& s : samples) {
        if (!BOOST_TEST_EQ(is(s.text), s.valid)) {
            std::cerr << name << ": \"" << s.text << "\"\n";
        }
        BOOST_TEST_EQ(is(s.text), urlg::parse(s.text, rule).has_value());
    }
}

/** `n` written with two digits, "07" for 7. */
std::string two_digits(int n) {
    return (n < 10 ? "0" : "") + std::to_string(n);
}

void rfc_3339_examples() {
    // RFC 3339 5.8, each of its five examples.
    constexpr std::array examples{
        sample{.text = "1985-04-12T23:20:50.52Z", .valid = true},
        sample{.text = "1996-12-19T16:39:57-08:00", .valid = true},
        sample{.text = "1990-12-31T23:59:60Z", .valid = true},
        sample{.text = "1990-12-31T15:59:60-08:00", .valid = true},
        sample{.text = "1937-01-01T12:00:27.87+00:20", .valid = true},
    };
    const auto is = [](std::string_view s) { return lexical::is_date_time(s); };
    check("RFC 3339 5.8", examples, is, fg::date_time_rule);
}

void every_month_ends_where_it_should() {
    constexpr std::array<int, 12> last{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int month = 1; month <= 12; ++month) {
        const int days = last[static_cast<std::size_t>(month - 1)];
        BOOST_TEST(lexical::is_date("2021-" + two_digits(month) + "-" + two_digits(days)));
        BOOST_TEST(!lexical::is_date("2021-" + two_digits(month) + "-" + two_digits(days + 1)));
        BOOST_TEST_EQ(webcpp::lexical::detail::days_in_month(2021, month), days);
    }
    for (const std::string_view refused : {"2021-00-01", "2021-13-01", "2021-01-00"}) {
        BOOST_TEST(!lexical::is_date(refused));
    }
    // A month that does not exist has no days.
    BOOST_TEST_EQ(webcpp::lexical::detail::days_in_month(2021, 0), 0);
    BOOST_TEST_EQ(webcpp::lexical::detail::days_in_month(2021, 13), 0);
}

void a_date_has_four_digits_then_two_and_two() {
    constexpr std::array dates{
        sample{.text = "0000-01-01", .valid = true},
        sample{.text = "9999-12-31", .valid = true},
        sample{.text = "202-01-01", .valid = false},
        sample{.text = "12021-01-01", .valid = false},
        sample{.text = "2021-001-01", .valid = false},
        sample{.text = "2021-01-001", .valid = false},
        sample{.text = "2021-01-1", .valid = false},
        sample{.text = "2021/01/01", .valid = false},
    };
    const auto is = [](std::string_view s) { return lexical::is_date(s); };
    check("date digits", dates, is, fg::date_rule);
}

void february_29_in_gregorian_leap_years_only() {
    constexpr std::array leap_days{
        sample{.text = "0400-02-29", .valid = true},   // divisible by 400
        sample{.text = "2000-02-29", .valid = true},   //
        sample{.text = "2020-02-29", .valid = true},   // by 4, not by 100
        sample{.text = "0100-02-29", .valid = false},  // by 100, not by 400
        sample{.text = "2100-02-29", .valid = false},  //
        sample{.text = "2021-02-29", .valid = false},  // not by 4
    };
    const auto is = [](std::string_view s) { return lexical::is_date(s); };
    check("leap years", leap_days, is, fg::date_rule);
}

void leap_seconds_at_each_offset() {
    // Second 60 is valid when the time, moved to UTC, is 23:59.
    constexpr std::array allowed_samples{
        sample{.text = "23:59:60Z", .valid = true},
        sample{.text = "23:59:60+00:00", .valid = true},
        sample{.text = "23:59:60-00:00", .valid = true},
        sample{.text = "15:59:60-08:00", .valid = true},
        sample{.text = "00:29:60-23:30", .valid = true},
        sample{.text = "01:29:60+01:30", .valid = true},
        sample{.text = "23:29:60+23:30", .valid = true},
        sample{.text = "23:59:60+01:00", .valid = false},
        sample{.text = "23:59:60-00:30", .valid = false},
        sample{.text = "22:59:60Z", .valid = false},
        sample{.text = "23:58:60Z", .valid = false},
        sample{.text = "23:59:61Z", .valid = false},
    };
    const auto allowed = [](std::string_view s) { return lexical::is_time(s); };
    check("leap seconds allowed", allowed_samples, allowed, fg::time_rule);
    constexpr std::array refused_samples{
        sample{.text = "23:59:60Z", .valid = false},
        sample{.text = "15:59:60-08:00", .valid = false},
        sample{.text = "00:29:60-23:30", .valid = false},
        sample{.text = "23:59:59Z", .valid = true},
    };
    const auto refused = [](std::string_view s) {
        return lexical::is_time(s, lexical::leap_seconds::refused);
    };
    check("leap seconds refused", refused_samples, refused,
          fg::time_rule_t{lexical::leap_seconds::refused});
}

void times_and_their_parts() {
    constexpr std::array times{
        sample{.text = "08:30:06z", .valid = true},
        sample{.text = "08:30:06.1Z", .valid = true},
        sample{.text = "08:30:06.123456789012345678901234567890Z", .valid = true},
        sample{.text = "00:00:00+23:59", .valid = true},
        sample{.text = "24:00:00Z", .valid = false},
        sample{.text = "00:60:00Z", .valid = false},
        sample{.text = "01:02:03+24:00", .valid = false},
        sample{.text = "01:02:03+00:60", .valid = false},
        sample{.text = "08:30:06.Z", .valid = false},
        sample{.text = "08:30:06+01", .valid = false},
        sample{.text = "08:30:06+0130", .valid = false},
        sample{.text = "12:00:00", .valid = false},
        sample{.text = "8:30:06Z", .valid = false},
        sample{.text = "08:30:06Z ", .valid = false},
    };
    const auto is = [](std::string_view s) { return lexical::is_time(s); };
    check("times", times, is, fg::time_rule);
}

void date_times_in_either_case() {
    constexpr std::array date_times{
        sample{.text = "1963-06-19t08:30:06.283185z", .valid = true},
        sample{.text = "1963-06-19T08:30:06Z", .valid = true},
        sample{.text = "1963-06-19 08:30:06Z", .valid = false},
        sample{.text = "1963-06-19T08:30Z", .valid = false},
        sample{.text = "2021-02-29T00:00:00Z", .valid = false},
        sample{.text = "1963-06-19T08:30:06Ztail", .valid = false},
    };
    const auto is = [](std::string_view s) { return lexical::is_date_time(s); };
    check("date-times", date_times, is, fg::date_time_rule);
}

void durations_of_appendix_a() {
    constexpr std::array durations{
        sample{.text = "P1Y", .valid = true},             // each designator alone
        sample{.text = "P1M", .valid = true},             //
        sample{.text = "P1W", .valid = true},             //
        sample{.text = "P1D", .valid = true},             //
        sample{.text = "PT1H", .valid = true},            //
        sample{.text = "PT1M", .valid = true},            //
        sample{.text = "PT1S", .valid = true},            //
        sample{.text = "P1Y2M3DT4H5M6S", .valid = true},  // every one, in order
        sample{.text = "P1DT12H", .valid = true},         // a date part and a time part
        sample{.text = "PT1H30M", .valid = true},         //
        sample{.text = "P0D", .valid = true},             // zero
        sample{.text = "P", .valid = false},              // nothing after P or T
        sample{.text = "PT", .valid = false},             //
        sample{.text = "P1DT", .valid = false},           //
        sample{.text = "P1H", .valid = false},            // a designator on the wrong side of T
        sample{.text = "PT1D", .valid = false},           //
        sample{.text = "P1Y1W", .valid = false},          // weeks with anything else
        sample{.text = "P1WT1H", .valid = false},         //
        sample{.text = "P1Y2D", .valid = false},          // a part skipped
        sample{.text = "PT1H2S", .valid = false},         //
        sample{.text = "P1.5D", .valid = false},          // a fraction, a sign
        sample{.text = "P-1D", .valid = false},           //
        sample{.text = "-P1D", .valid = false},           //
        sample{.text = "P1D1Y", .valid = false},          // out of order
        sample{.text = "PT1S1M", .valid = false},         //
    };
    const auto is = [](std::string_view s) { return lexical::is_duration(s); };
    check("durations", durations, is, fg::duration_rule);
}

void duration_designators_in_either_case() {
    // RFC 5234 2.3: the quoted letters of RFC 3339 appendix A's ABNF are case-insensitive.
    constexpr std::array lowercase{
        sample{.text = "p1d", .valid = true},
        sample{.text = "pt1h", .valid = true},
        sample{.text = "P1y2m3dT4h5m6s", .valid = true},
        sample{.text = "p1w", .valid = true},
    };
    const auto is = [](std::string_view s) { return lexical::is_duration(s); };
    check("lowercase designators", lowercase, is, fg::duration_rule);
}

void nul_is_no_part_of_a_date_or_a_time() {
    BOOST_TEST(!lexical::is_date(std::string_view("2020-01-01\0", 11)));
    BOOST_TEST(!lexical::is_time(std::string_view("12:00:00\0Z", 10)));
    BOOST_TEST(!lexical::is_date_time(std::string_view("2020-01-01T12:00:00Z\0", 21)));
    BOOST_TEST(!lexical::is_duration(std::string_view("P1\0D", 4)));
    // A NUL after a whole time or duration: what precedes it would be valid alone.
    BOOST_TEST(!lexical::is_time(std::string_view("12:00:00Z\0", 10)));
    BOOST_TEST(!lexical::is_duration(std::string_view("P1D\0", 4)));
}

/** Checks that `rule` refuses `text` with `expected` and leaves the iterator where it was. */
template <class Rule>
void leaves_its_input(const Rule& rule, std::string_view text, urlg::error expected) {
    char const* it = text.data();
    const auto r = rule.parse(it, text.data() + text.size());
    if (!BOOST_TEST(r.has_error() && r.error() == expected)) {
        std::cerr << "refusal of \"" << text << "\"\n";
    }
    BOOST_TEST(it == text.data());
}

void a_rule_that_fails_leaves_its_input() {
    leaves_its_input(fg::date_rule, "x2021-01-01", urlg::error::mismatch);
    leaves_its_input(fg::date_rule, "2021-02-29", urlg::error::invalid);
    leaves_its_input(fg::date_rule, "2021-1-01", urlg::error::invalid);
    leaves_its_input(fg::time_rule, "T12:00:00Z", urlg::error::mismatch);
    leaves_its_input(fg::time_rule, "12:00:00", urlg::error::invalid);
    leaves_its_input(fg::time_rule_t{lexical::leap_seconds::refused}, "23:59:60Z",
                     urlg::error::invalid);
    leaves_its_input(fg::date_time_rule, "2021-01-01X00:00:00Z", urlg::error::invalid);
    leaves_its_input(fg::duration_rule, "1D", urlg::error::mismatch);
    leaves_its_input(fg::duration_rule, "PT", urlg::error::invalid);
}

void rules_stop_where_their_grammar_does() {
    constexpr auto interval =
        urlg::tuple_rule(fg::date_rule, urlg::squelch(urlg::delim_rule('/')), fg::date_rule);
    const auto dates = urlg::parse("2020-01-01/2021-02-03", interval);
    if (BOOST_TEST(dates.has_value())) {
        BOOST_TEST_EQ(std::get<0>(*dates), "2020-01-01");
        BOOST_TEST_EQ(std::get<1>(*dates), "2021-02-03");
    }
    const auto timed = urlg::parse(
        "08:30:06Z/P1D",
        urlg::tuple_rule(fg::time_rule, urlg::squelch(urlg::delim_rule('/')), fg::duration_rule));
    if (BOOST_TEST(timed.has_value())) {
        BOOST_TEST_EQ(std::get<0>(*timed), "08:30:06Z");
        BOOST_TEST_EQ(std::get<1>(*timed), "P1D");
    }
    const auto leftover = urlg::parse("2020-01-01x", fg::date_rule);
    BOOST_TEST(leftover.has_error() && leftover.error() == urlg::error::leftover);
    // Appendix A keeps years, months and days contiguous: of P1Y2D, the rule takes P1Y alone.
    const std::string_view skipped = "P1Y2D";
    char const* it = skipped.data();
    const auto years = fg::duration_rule.parse(it, skipped.data() + skipped.size());
    if (BOOST_TEST(years.has_value())) {
        BOOST_TEST_EQ(*years, "P1Y");
    }
    BOOST_TEST(it == skipped.data() + 3);
}

}  // namespace

int main() {
    rfc_3339_examples();
    every_month_ends_where_it_should();
    a_date_has_four_digits_then_two_and_two();
    february_29_in_gregorian_leap_years_only();
    leap_seconds_at_each_offset();
    times_and_their_parts();
    date_times_in_either_case();
    durations_of_appendix_a();
    duration_designators_in_either_case();
    nul_is_no_part_of_a_date_or_a_time();
    a_rule_that_fails_leaves_its_input();
    rules_stop_where_their_grammar_does();
    return boost::report_errors();
}
