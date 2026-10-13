// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Dates, times and durations: RFC 3339's `date-time`, `full-date`, `full-time` and appendix A's
 `duration`, as validators and as Boost.URL rules.

 @see "Dates and times", in the guide.
*/
#ifndef WEBCPP_LEXICAL_DATE_TIME_HPP
#define WEBCPP_LEXICAL_DATE_TIME_HPP

#include <webcpp/lexical/config.hpp>

#include <webcpp/lexical/detail/rules.hpp>
#include <webcpp/lexical/grammar.hpp>

#include <boost/url/grammar/delim_rule.hpp>
#include <boost/url/grammar/digit_chars.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/lut_chars.hpp>
#include <boost/url/grammar/optional_rule.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/token_rule.hpp>
#include <boost/url/grammar/tuple_rule.hpp>
#include <boost/url/grammar/variant_rule.hpp>

#include <array>
#include <cstddef>
#include <string_view>

namespace webcpp::lexical {

/**
 Whether a time may hold second 60, a leap second.

 @see "Dates and times", in the guide.
*/
enum class leap_seconds {
    /** Second 60 is valid when the time moved to UTC is 23:59, as RFC 3339 5.7 allows. */
    allowed,
    /** Second 60 is never valid, as JSON Schema v1's `time` asks. */
    refused,
};

namespace detail {

namespace urlg = boost::urls::grammar;

/** The value of a short run of ASCII digits: a year, a month, an hour. */
constexpr int digits_value(grammar::text digits) noexcept {
    int value = 0;
    for (const char digit : digits) {
        value = (value * 10) + (digit - '0');
    }
    return value;
}

/** Whether `year` is a Gregorian leap year: divisible by 4, and not by 100 unless by 400. */
constexpr bool is_leap_year(int year) noexcept {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

/** The days of `month` in `year`, Gregorian (RFC 3339 5.7), and 0 for a month not 1 to 12. */
constexpr int days_in_month(int year, int month) noexcept {
    if (month < 1 || month > 12) {
        return 0;
    }
    constexpr std::array<int, 12> days{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return days[static_cast<std::size_t>(month - 1)];
}

/** Whether hh:mm:ss, `offset` minutes east of UTC, is a time of RFC 3339 5.6 and 5.7. */
constexpr bool is_valid_time(int hour, int minute, int second, int offset,
                             leap_seconds leap) noexcept {
    if (hour > 23 || minute > 59 || second > 60) {
        return false;
    }
    if (second < 60) {
        return true;
    }
    if (leap == leap_seconds::refused) {
        return false;
    }
    constexpr int minutes_per_day = 24 * 60;
    const int utc =
        ((((hour * 60) + minute - offset) % minutes_per_day) + minutes_per_day) % minutes_per_day;
    return utc == (23 * 60) + 59;
}

/** RFC 3339 5.6 full-date's shape, 4DIGIT "-" 2DIGIT "-" 2DIGIT: its year, month and day. */
inline constexpr auto full_date_shape =
    urlg::tuple_rule(exactly<4>(urlg::digit_chars), urlg::squelch(urlg::delim_rule('-')),
                     two_digits_rule, urlg::squelch(urlg::delim_rule('-')), two_digits_rule);

/**
 RFC 3339 5.6 partial-time's shape, hh ":" mm ":" ss ["." 1*DIGIT]: its hour, minute and
 second.
*/
inline constexpr auto partial_time_shape = urlg::tuple_rule(
    two_digits_rule, urlg::squelch(urlg::delim_rule(':')), two_digits_rule,
    urlg::squelch(urlg::delim_rule(':')), two_digits_rule,
    urlg::squelch(urlg::optional_rule(urlg::tuple_rule(urlg::squelch(urlg::delim_rule('.')),
                                                       urlg::token_rule(urlg::digit_chars)))));

/** RFC 3339 5.6 time-numoffset's shape, ("+" / "-") hh ":" mm: its sign, hours and minutes. */
inline constexpr auto numeric_offset_shape =
    urlg::tuple_rule(urlg::delim_rule(urlg::lut_chars("+-")), two_digits_rule,
                     urlg::squelch(urlg::delim_rule(':')), two_digits_rule);

/**
 Reads RFC 3339's time-offset at `it` as minutes east of UTC, and moves `it` past it.

 Tip: a rule's errors, with `it` unchanged: mismatch when no `Z`, `+` or `-` starts it, invalid
 when one did and the hours or the minutes broke their shape or range.
*/
inline grammar::result<int> parse_time_offset(char const*& it, char const* end) noexcept {
    if (it == end || (*it != 'Z' && *it != 'z' && *it != '+' && *it != '-')) {
        return urlg::error::mismatch;
    }
    if (*it == 'Z' || *it == 'z') {
        ++it;
        return 0;
    }
    char const* const first = it;
    const auto offset = urlg::parse(it, end, numeric_offset_shape);
    if (!offset) {
        it = first;
        return urlg::error::invalid;
    }
    const auto& [sign, hours_text, minutes_text] = offset.unsafe_value();
    const int hours = digits_value(hours_text);
    const int minutes = digits_value(minutes_text);
    if (hours > 23 || minutes > 59) {
        it = first;
        return urlg::error::invalid;
    }
    return (sign == "-" ? -1 : 1) * ((hours * 60) + minutes);
}

/** One designator of a duration, in either case (RFC 5234 2.3), matched and dropped. */
constexpr auto designator(char upper) noexcept {
    return urlg::squelch(urlg::delim_rule(urlg::lut_chars(upper) +
                                          urlg::lut_chars(static_cast<char>(upper - 'A' + 'a'))));
}

/** 1*DIGIT, any length: a duration's numbers are never converted, so none overflows. */
inline constexpr auto duration_digits_rule = urlg::token_rule(urlg::digit_chars);

// RFC 3339 appendix A, one rule per production, in the ABNF's order.

/** The production dur-second, 1*DIGIT "S". */
inline constexpr auto dur_second = urlg::tuple_rule(duration_digits_rule, designator('S'));

/** The production dur-minute, 1*DIGIT "M" [dur-second]. */
inline constexpr auto dur_minute =
    urlg::tuple_rule(duration_digits_rule, designator('M'), urlg::optional_rule(dur_second));

/** The production dur-hour, 1*DIGIT "H" [dur-minute]. */
inline constexpr auto dur_hour =
    urlg::tuple_rule(duration_digits_rule, designator('H'), urlg::optional_rule(dur_minute));

/** The production dur-time, "T" (dur-hour / dur-minute / dur-second). */
inline constexpr auto dur_time =
    urlg::tuple_rule(designator('T'), urlg::variant_rule(dur_hour, dur_minute, dur_second));

/** The production dur-day, 1*DIGIT "D". */
inline constexpr auto dur_day = urlg::tuple_rule(duration_digits_rule, designator('D'));

/** The production dur-week, 1*DIGIT "W". */
inline constexpr auto dur_week = urlg::tuple_rule(duration_digits_rule, designator('W'));

/** The production dur-month, 1*DIGIT "M" [dur-day]. */
inline constexpr auto dur_month =
    urlg::tuple_rule(duration_digits_rule, designator('M'), urlg::optional_rule(dur_day));

/** The production dur-year, 1*DIGIT "Y" [dur-month]. */
inline constexpr auto dur_year =
    urlg::tuple_rule(duration_digits_rule, designator('Y'), urlg::optional_rule(dur_month));

/** The production dur-date, (dur-day / dur-month / dur-year) [dur-time]. */
inline constexpr auto dur_date = urlg::tuple_rule(urlg::variant_rule(dur_day, dur_month, dur_year),
                                                  urlg::optional_rule(dur_time));

/** RFC 3339 appendix A's duration: "P" (dur-date / dur-time / dur-week). */
inline constexpr auto duration_shape =
    urlg::tuple_rule(designator('P'), urlg::variant_rule(dur_date, dur_time, dur_week));

}  // namespace detail

namespace grammar {

/**
 The rule of RFC 3339 5.6's `full-date`: a year, a month and a day that exists.

 `4DIGIT "-" 2DIGIT "-" 2DIGIT`, ASCII digits only; the month 01 to 12, the day 01 to the month's
 length, February 29 only in a Gregorian leap year. It stops after the day.

 @see "Dates and times", in the guide.
*/
struct date_rule_t {
    /** The rule yields the text it matched. */
    using value_type = text;

    /**
     Matches a `full-date` at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The text matched; `error::mismatch` when no digit starts it; `error::invalid` when it
     started and broke a constraint (Boost.URL's grammar errors).
    */
    [[nodiscard]] result<text> parse(char const*& it, char const* end) const noexcept;
};

/**
 The rule of RFC 3339 5.6's `full-time`: a time of day and its offset from UTC.

 Hours 00 to 23, minutes 00 to 59, seconds 00 to 60 with any number of fraction digits, then `Z`
 or `+hh:mm` or `-hh:mm` (hh 00 to 23, mm 00 to 59); `Z` in either case. Second 60 is valid only
 under @ref leap_seconds::allowed and when the time moved to UTC is 23:59. It stops after the
 offset.

 @see "Dates and times", in the guide.
*/
struct time_rule_t {
    /** The rule yields the text it matched. */
    using value_type = text;

    /** Whether second 60 may be valid. */
    leap_seconds leap = leap_seconds::allowed;

    /**
     Matches a `full-time` at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The text matched; `error::mismatch` when no digit starts it; `error::invalid` when it
     started and broke a constraint.
    */
    [[nodiscard]] result<text> parse(char const*& it, char const* end) const noexcept;
};

/**
 The rule of RFC 3339 5.6's `date-time`: a `full-date`, `T` in either case, a `full-time`.

 Leap seconds are allowed, as every draft of JSON Schema, v1 included, says of `date-time`. It
 stops after the offset.

 @see "Dates and times", in the guide.
*/
struct date_time_rule_t {
    /** The rule yields the text it matched. */
    using value_type = text;

    /**
     Matches a `date-time` at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The text matched; `error::mismatch` when no digit starts it; `error::invalid` when it
     started and broke a constraint.
    */
    [[nodiscard]] result<text> parse(char const*& it, char const* end) const noexcept;
};

/**
 The rule of RFC 3339 appendix A's `duration`, exactly: `P`, then a date part, a time part, or
 weeks alone.

 Years, months and days in that order and each optional after the first given, then `T` and
 hours, minutes and seconds likewise; or a number of weeks alone. No fraction, no sign, any
 number of digits. The designators are letters of the ABNF, so either case matches (RFC 5234
 2.3). It stops where the ABNF does: after `P1Y`, the `2D` of `P1Y2D` is not its.

 @see "Dates and times", in the guide.
*/
struct duration_rule_t {
    /** The rule yields the text it matched. */
    using value_type = text;

    /**
     Matches a `duration` at `it`.

     @param it The start of the input: past the match on success, else unchanged.
     @param end The end of the input.
     @return The text matched; `error::mismatch` when no `P` starts it; `error::invalid` when it
     started and broke the grammar.
    */
    [[nodiscard]] result<text> parse(char const*& it, char const* end) const noexcept;
};

/** The rule of RFC 3339's `full-date`. */
inline constexpr date_rule_t date_rule{};

/**
 The rule of RFC 3339's `full-time`, leap seconds allowed; `time_rule_t{leap_seconds::refused}`
 refuses them.
*/
inline constexpr time_rule_t time_rule{};

/** The rule of RFC 3339's `date-time`. */
inline constexpr date_time_rule_t date_time_rule{};

/** The rule of RFC 3339 appendix A's `duration`. */
inline constexpr duration_rule_t duration_rule{};

// A parse that reads no member stays a const member, as Boost.URL's Rule concept and the
// rules' declarations above have it.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline result<text> date_rule_t::parse(char const*& it, char const* end) const noexcept {
    if (it == end || !detail::urlg::digit_chars(*it)) {
        return detail::urlg::error::mismatch;
    }
    char const* const first = it;
    const auto date = detail::urlg::parse(it, end, detail::full_date_shape);
    if (!date) {
        it = first;
        return detail::urlg::error::invalid;
    }
    const auto& [year_text, month_text, day_text] = date.unsafe_value();
    const int year = detail::digits_value(year_text);
    const int month = detail::digits_value(month_text);
    const int day = detail::digits_value(day_text);
    if (month < 1 || month > 12 || day < 1 || day > detail::days_in_month(year, month)) {
        it = first;
        return detail::urlg::error::invalid;
    }
    return text(first, static_cast<std::size_t>(it - first));
}

inline result<text> time_rule_t::parse(char const*& it, char const* end) const noexcept {
    if (it == end || !detail::urlg::digit_chars(*it)) {
        return detail::urlg::error::mismatch;
    }
    char const* const first = it;
    const auto time = detail::urlg::parse(it, end, detail::partial_time_shape);
    if (!time) {
        it = first;
        return detail::urlg::error::invalid;
    }
    const auto offset = detail::parse_time_offset(it, end);
    if (!offset) {
        it = first;
        return detail::urlg::error::invalid;
    }
    const auto& [hour, minute, second] = time.unsafe_value();
    if (!detail::is_valid_time(detail::digits_value(hour), detail::digits_value(minute),
                               detail::digits_value(second), offset.unsafe_value(), leap)) {
        it = first;
        return detail::urlg::error::invalid;
    }
    return text(first, static_cast<std::size_t>(it - first));
}

// A parse that reads no member stays a const member, as Boost.URL's Rule concept and the
// rules' declarations above have it.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline result<text> date_time_rule_t::parse(char const*& it, char const* end) const noexcept {
    char const* const first = it;
    if (const auto date = date_rule.parse(it, end); !date) {
        return date.error();
    }
    if (it == end || (*it != 'T' && *it != 't')) {
        it = first;
        return detail::urlg::error::invalid;
    }
    ++it;
    if (const auto time = time_rule.parse(it, end); !time) {
        it = first;
        return detail::urlg::error::invalid;
    }
    return text(first, static_cast<std::size_t>(it - first));
}

// A parse that reads no member stays a const member, as Boost.URL's Rule concept and the
// rules' declarations above have it.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline result<text> duration_rule_t::parse(char const*& it, char const* end) const noexcept {
    if (it == end || (*it != 'P' && *it != 'p')) {
        return detail::urlg::error::mismatch;
    }
    char const* const first = it;
    if (const auto duration = detail::urlg::parse(it, end, detail::duration_shape); !duration) {
        it = first;
        return detail::urlg::error::invalid;
    }
    return text(first, static_cast<std::size_t>(it - first));
}

}  // namespace grammar

/**
 Whether `s` is an RFC 3339 `date-time`, JSON Schema's `date-time`.

 @param s Any byte string.
 @return `parse(s, grammar::date_time_rule).has_value()`.
 @see "Dates and times", in the guide.
*/
[[nodiscard]] inline bool is_date_time(std::string_view s) noexcept {
    return detail::urlg::parse(s, grammar::date_time_rule).has_value();
}

/**
 Whether `s` is an RFC 3339 `full-date`, JSON Schema's `date`.

 @param s Any byte string.
 @return `parse(s, grammar::date_rule).has_value()`.
 @see "Dates and times", in the guide.
*/
[[nodiscard]] inline bool is_date(std::string_view s) noexcept {
    return detail::urlg::parse(s, grammar::date_rule).has_value();
}

/**
 Whether `s` is an RFC 3339 `full-time`, JSON Schema's `time`.

 @param s Any byte string.
 @param leap Whether second 60 may be valid: @ref leap_seconds::allowed for drafts 7 to 2020-12,
 @ref leap_seconds::refused for v1.
 @return `parse(s, grammar::time_rule_t{leap}).has_value()`.
 @see "Dates and times", in the guide.
*/
[[nodiscard]] inline bool is_time(std::string_view s,
                                  leap_seconds leap = leap_seconds::allowed) noexcept {
    return detail::urlg::parse(s, grammar::time_rule_t{leap}).has_value();
}

/**
 Whether `s` is an RFC 3339 appendix A `duration`, JSON Schema's `duration`.

 @param s Any byte string.
 @return `parse(s, grammar::duration_rule).has_value()`.
 @see "Dates and times", in the guide.
*/
[[nodiscard]] inline bool is_duration(std::string_view s) noexcept {
    return detail::urlg::parse(s, grammar::duration_rule).has_value();
}

}  // namespace webcpp::lexical

#endif  // WEBCPP_LEXICAL_DATE_TIME_HPP
