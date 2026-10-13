// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The syntax of ECMA-262's Pattern[+UnicodeMode] and its early errors: a recursive-descent reader
// over well-formed UTF-8 that answers whether a pattern is one, and allocates nothing. What it
// needs to know of the whole pattern, the number of its groups, the groups a name names and where
// a group sits among the disjunctions, it reads again from the text when it needs it.

#ifndef WEBCPP_LEXICAL_DETAIL_REGEX_VALIDATOR_HPP
#define WEBCPP_LEXICAL_DETAIL_REGEX_VALIDATOR_HPP

#include <webcpp/lexical/config.hpp>

#include <webcpp/lexical/detail/regex_tables.hpp>
#include <webcpp/lexical/detail/regex_unicode.hpp>
#include <webcpp/lexical/detail/utf8.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace webcpp::lexical::detail {

/** What the validator answers of a pattern. */
enum class regex_verdict : std::uint8_t {
    /** An ECMA-262 pattern in u mode. */
    valid,
    /** Not an ECMA-262 pattern in u mode: a SyntaxError in JavaScript. */
    invalid,
    /** Nested deeper than the validator reads, and not judged. */
    beyond_bounds,
};

/** A pattern's verdict, and why when it is not valid. */
struct regex_check {
    /** The verdict. */
    regex_verdict verdict = regex_verdict::valid;
    /** Why the pattern is not valid, a literal; empty when it is. */
    std::string_view why;
};

/** The modifiers in force that the syntax reads: whether a pattern's text is under them. */
struct regex_place {
    /** Where the group that opens the disjunction starts, `npos` for the pattern's own. */
    std::size_t disjunction = std::string_view::npos;
    /** Which of the disjunction's alternatives holds the place, from 0. */
    std::size_t alternative = 0;
};

/** The code point at byte `at` of the well-formed UTF-8 `s` and the byte after it, or nothing. */
constexpr std::optional<char32_t> code_point_at(std::string_view s, std::size_t at,
                                                std::size_t* next = nullptr) noexcept {
    std::size_t i = at;
    char32_t cp = 0;
    if (!decode_utf8(s, i, cp)) {
        return std::nullopt;
    }
    if (next != nullptr) {
        *next = i;
    }
    return cp;
}

/** The value of the hexadecimal digit `c`, or nothing. */
constexpr std::optional<unsigned> hex_value(char32_t c) noexcept {
    if (c >= '0' && c <= '9') {
        return static_cast<unsigned>(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return static_cast<unsigned>(c - 'a') + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return static_cast<unsigned>(c - 'A') + 10;
    }
    return std::nullopt;
}

/** `s.substr(at, count)` that cannot throw: the bytes of `s` from `at`, none past its end. */
constexpr std::string_view slice(std::string_view s, std::size_t at,
                                 std::size_t count = std::string_view::npos) noexcept {
    if (at >= s.size()) {
        return {};
    }
    return {s.data() + at, std::min(count, s.size() - at)};
}

/** `count` hexadecimal digits at byte `at` of `s`, as a number, moving `at` past them. */
constexpr std::optional<char32_t> hex_digits(std::string_view s, std::size_t& at,
                                             std::size_t count) noexcept {
    char32_t value = 0;
    for (std::size_t k = 0; k < count; ++k) {
        const std::optional<unsigned> digit =
            at + k < s.size() ? hex_value(static_cast<unsigned char>(s[at + k])) : std::nullopt;
        if (!digit) {
            return std::nullopt;
        }
        value = static_cast<char32_t>((value * 16) + *digit);
    }
    at += count;
    return value;
}

/**
 The code point of the escape after `\\u` at byte `at` of `s`, `{X...}` up to 10FFFF or `XXXX`, an
 escaped surrogate pair joined into one, moving `at` past it; nothing for a malformed one.
*/
constexpr std::optional<char32_t> unicode_escape(std::string_view s, std::size_t& at) noexcept {
    if (at < s.size() && s[at] == '{') {
        std::size_t i = at + 1;
        char32_t value = 0;
        std::size_t digits = 0;
        for (; i < s.size() && s[i] != '}'; ++i, ++digits) {
            const std::optional<unsigned> digit = hex_value(static_cast<unsigned char>(s[i]));
            if (!digit) {
                return std::nullopt;
            }
            value = static_cast<char32_t>((value * 16) + *digit);
            if (value > 0x10FFFF) {
                return std::nullopt;
            }
        }
        if (digits == 0 || i == s.size()) {
            return std::nullopt;
        }
        at = i + 1;
        return value;
    }
    const std::optional<char32_t> high = hex_digits(s, at, 4);
    if (!high || *high < 0xD800 || *high > 0xDBFF || slice(s, at, 2) != "\\u") {
        return high;
    }
    std::size_t after = at + 2;
    const std::optional<char32_t> low = hex_digits(s, after, 4);
    if (!low || *low < 0xDC00 || *low > 0xDFFF) {
        return high;
    }
    at = after;
    return static_cast<char32_t>(0x10000 + ((*high - 0xD800) << 10U) + (*low - 0xDC00));
}

/**
 The next code point of a group's name at byte `at` of `s`, a `\\u` escape decoded, moving `at`
 past it; nothing at the name's `>`, at the end, or on a malformed escape.
*/
constexpr std::optional<char32_t> name_code_point(std::string_view s, std::size_t& at) noexcept {
    if (at >= s.size() || s[at] == '>') {
        return std::nullopt;
    }
    if (s[at] != '\\') {
        return code_point_at(s, at, &at);
    }
    if (at + 1 >= s.size() || s[at + 1] != 'u') {
        return std::nullopt;
    }
    at += 2;
    return unicode_escape(s, at);
}

/** Whether the names that start at bytes `a` and `b` of `s`, after their `<`, are one. */
constexpr bool same_name(std::string_view s, std::size_t a, std::size_t b) noexcept {
    while (true) {
        const std::optional<char32_t> x = name_code_point(s, a);
        const std::optional<char32_t> y = name_code_point(s, b);
        if (x != y) {
            return false;
        }
        if (!x) {
            return true;
        }
    }
}

/**
 What the pattern's text holds at a byte, read by `next_token`: a parenthesis, a `|`, or anything
 else, an escape and a whole class each one token.
*/
enum class regex_token : std::uint8_t {
    /** A `(`. */
    open,
    /** A `)`. */
    close,
    /** A `|`. */
    bar,
    /** Anything else: a code point, an escape or a class. */
    other,
};

/** The token at byte `at` of `s`, moving `at` past it. */
constexpr regex_token next_token(std::string_view s, std::size_t& at) noexcept {
    const char c = s[at];
    if (c == '\\') {
        at += 1;
        if (at < s.size()) {
            code_point_at(s, at, &at);
        }
        return regex_token::other;
    }
    if (c == '[') {
        for (++at; at < s.size() && s[at] != ']'; ++at) {
            at += s[at] == '\\' ? 1 : 0;
        }
        at = std::min(at + 1, s.size());
        return regex_token::other;
    }
    code_point_at(s, at, &at);
    switch (c) {
        case '(': return regex_token::open;
        case ')': return regex_token::close;
        case '|': return regex_token::bar;
        default: return regex_token::other;
    }
}

/** Whether the `(` at byte `at` of `s` opens a named group, `(?<` and no `=` or `!` after. */
constexpr bool opens_named_group(std::string_view s, std::size_t at) noexcept {
    return slice(s, at, 3) == "(?<" && at + 3 < s.size() && s[at + 3] != '=' && s[at + 3] != '!';
}

/** Whether the `(` at byte `at` of `s` opens a group that captures. */
constexpr bool opens_capture(std::string_view s, std::size_t at) noexcept {
    return slice(s, at, 2) != "(?" || opens_named_group(s, at);
}

/**
 The reader of one pattern, well-formed UTF-8: `run` reads it once and answers it.

 Tip: the reader descends into a group by calling itself, from `disjunction` through `group` and
 back, which `max_nesting` bounds; each of those functions is exempt from misc-no-recursion for
 that reason.
*/
class regex_validator {
public:
    /** The deepest groups may nest that the reader reads; deeper is `beyond_bounds`. */
    static constexpr std::size_t nesting_limit = 64;

    /** A reader of `pattern`, well-formed UTF-8, judging groups nested at most `max_nesting`. */
    constexpr regex_validator(std::string_view pattern, std::size_t max_nesting) noexcept
        : p_(pattern), max_nesting_(std::min(max_nesting, nesting_limit)) {}

    /** Reads the pattern and answers it. */
    constexpr regex_check run() noexcept {
        count_groups();
        disjunction({});
        if (!failed() && more()) {
            fail(peek() == ')' ? "an unmatched )" : "an unexpected character");
        }
        return check_;
    }

private:
    /** The modifiers in force: ECMA-262's flags. Only whether i is on matters to the syntax. */
    struct flags {
        bool i = false;
    };

    static constexpr char32_t none = 0xFFFFFFFF;
    // The longest \p{...} body that may name a property: past it, none does.
    static constexpr std::size_t longest_property = 64;

    std::string_view p_;
    std::size_t i_ = 0;
    std::size_t max_nesting_;
    std::size_t depth_ = 0;
    std::size_t groups_ = 0;
    regex_check check_;

    [[nodiscard]] constexpr bool failed() const noexcept {
        return check_.verdict != regex_verdict::valid;
    }

    constexpr void fail(std::string_view why,
                        regex_verdict verdict = regex_verdict::invalid) noexcept {
        if (failed()) {
            return;
        }
        check_ = {.verdict = verdict, .why = why};
    }

    [[nodiscard]] constexpr bool more() const noexcept { return i_ < p_.size(); }

    // The code point `ahead` code points after i_, or none past the end.
    [[nodiscard]] constexpr char32_t peek(std::size_t ahead = 0) const noexcept {
        std::size_t at = i_;
        for (std::size_t k = 0; k < ahead && at < p_.size(); ++k) {
            code_point_at(p_, at, &at);
        }
        if (at >= p_.size()) {
            return none;
        }
        return code_point_at(p_, at).value_or(none);
    }

    constexpr char32_t take() noexcept {
        const char32_t c = peek();
        if (more()) {
            code_point_at(p_, i_, &i_);
        }
        return c;
    }

    constexpr bool eat(char32_t c) noexcept {
        if (peek() != c) {
            return false;
        }
        take();
        return true;
    }

    // The whole pattern.

    // Counts the groups that capture: a backreference may name one after it.
    constexpr void count_groups() noexcept {
        for (std::size_t at = 0; at < p_.size();) {
            const std::size_t token = at;
            if (next_token(p_, at) == regex_token::open && opens_capture(p_, token)) {
                ++groups_;
            }
        }
    }

    // Whether some group of the pattern is named as the name at byte `name` is.
    [[nodiscard]] constexpr bool has_group_named(std::size_t name) const noexcept {
        for (std::size_t at = 0; at < p_.size();) {
            const std::size_t token = at;
            if (next_token(p_, at) == regex_token::open && opens_named_group(p_, token) &&
                same_name(p_, token + 3, name)) {
                return true;
            }
        }
        return false;
    }

    // The disjunctions that hold the `(` at byte `target`, outermost first, into `places`; how
    // many there are.
    [[nodiscard]] constexpr std::size_t places_of(
        std::size_t target, std::array<regex_place, nesting_limit + 1>& places) const noexcept {
        std::size_t count = 1;
        places[0] = {};
        for (std::size_t at = 0; at < target;) {
            const std::size_t token = at;
            switch (next_token(p_, at)) {
                case regex_token::open:
                    places[count++] = {.disjunction = token, .alternative = 0};
                    break;
                case regex_token::close: --count; break;
                case regex_token::bar: ++places[count - 1].alternative; break;
                case regex_token::other: break;
            }
        }
        return count;
    }

    // ECMA-262 2025 (22.2.1.1): two groups may share a name only when no match can hold both,
    // that is when some disjunction holds them in different alternatives. Checking each named
    // group, at `open`, against the last one before it of the same name checks every pair.
    constexpr void check_duplicate_name(std::size_t open) noexcept {
        std::size_t last = std::string_view::npos;
        for (std::size_t at = 0; at < open;) {
            const std::size_t token = at;
            if (next_token(p_, at) == regex_token::open && opens_named_group(p_, token) &&
                same_name(p_, token + 3, open + 3)) {
                last = token;
            }
        }
        if (last == std::string_view::npos) {
            return;
        }
        std::array<regex_place, nesting_limit + 1> earlier{};
        std::array<regex_place, nesting_limit + 1> later{};
        const std::size_t levels = std::min(places_of(last, earlier), places_of(open, later));
        for (std::size_t level = 0; level < levels; ++level) {
            if (earlier[level].disjunction != later[level].disjunction) {
                break;
            }
            if (earlier[level].alternative != later[level].alternative) {
                return;
            }
        }
        fail("a duplicate group name");
    }

    // Names and escapes.

    // RegExpIdentifierName, then `>`; at i_.
    constexpr bool group_name() noexcept {
        std::size_t at = i_;
        for (bool first = true;; first = false) {
            if (at < p_.size() && p_[at] == '>') {
                if (first) {
                    fail("an empty group name");
                    return false;
                }
                i_ = at + 1;
                return true;
            }
            const std::optional<char32_t> c = name_code_point(p_, at);
            if (!c) {
                fail("an unterminated group name, or a malformed escape in one");
                return false;
            }
            if (first ? !is_name_start(*c) : !is_name_part(*c)) {
                fail("a group name's character that no identifier holds there");
                return false;
            }
        }
    }

    // Whether `\p{name}` names a class: a General_Category value or a binary property.
    [[nodiscard]] static constexpr bool lone_property(std::string_view name) noexcept {
        return has_name(regex_tables::general_category_names, name) ||
               has_name(regex_tables::binary_names, name);
    }

    // Whether `\p{name=value}` names a class: of General_Category, Script or Script_Extensions.
    [[nodiscard]] static constexpr bool property_value(std::string_view name,
                                                       std::string_view value) noexcept {
        if (name == "General_Category" || name == "gc") {
            return has_name(regex_tables::general_category_names, value);
        }
        const bool script =
            name == "Script" || name == "sc" || name == "Script_Extensions" || name == "scx";
        return script && has_name(regex_tables::script_names, value);
    }

    // `\p{...}` or `\P{...}`, after the p or the P.
    constexpr bool property() noexcept {
        if (!eat('{')) {
            fail("a \\p without {");
            return false;
        }
        const std::size_t first = i_;
        while (more() && peek() != '}') {
            take();
        }
        if (!eat('}')) {
            fail("an unterminated \\p{");
            return false;
        }
        const std::string_view body = slice(p_, first, i_ - 1 - first);
        const std::size_t equals = body.find('=');
        const bool found = body.size() <= longest_property &&
                           (equals == std::string_view::npos
                                ? lone_property(body)
                                : property_value(slice(body, 0, equals), slice(body, equals + 1)));
        if (!found) {
            fail("an unknown property or value in \\p{...}");
        }
        return found;
    }

    // `\u` and its escape, after the u, as a code point.
    constexpr std::optional<char32_t> unicode() noexcept {
        const std::optional<char32_t> c = unicode_escape(p_, i_);
        if (!c) {
            fail("a malformed \\u escape");
        }
        return c;
    }

    // Disjunctions.

    // NOLINTNEXTLINE(misc-no-recursion)
    constexpr void disjunction(flags f) noexcept {
        alternative(f);
        while (!failed() && eat('|')) {
            alternative(f);
        }
    }

    // NOLINTNEXTLINE(misc-no-recursion)
    constexpr void alternative(flags f) noexcept {
        while (!failed() && more() && peek() != '|' && peek() != ')') {
            term(f);
        }
    }

    // One more level of groups; false, failed, past the bound.
    constexpr bool enter() noexcept {
        if (++depth_ > max_nesting_) {
            fail("groups nested deeper than the bound", regex_verdict::beyond_bounds);
            return false;
        }
        return true;
    }

    // NOLINTNEXTLINE(misc-no-recursion)
    constexpr void term(flags f) noexcept {
        if (assertion(f)) {
            no_quantifier();
            return;
        }
        if (atom(f)) {
            quantifier();
        }
    }

    // An assertion at i_: `^`, `$`, `\b`, `\B` or a lookaround; false when none is there.
    // NOLINTNEXTLINE(misc-no-recursion)
    constexpr bool assertion(flags f) noexcept {
        const char32_t c = peek();
        if (c == '^' || c == '$') {
            take();
            return true;
        }
        if (c == '\\' && (peek(1) == 'b' || peek(1) == 'B')) {
            i_ += 2;
            return true;
        }
        if (c != '(' || peek(1) != '?') {
            return false;
        }
        const bool ahead = peek(2) == '=' || peek(2) == '!';
        const bool behind = peek(2) == '<' && (peek(3) == '=' || peek(3) == '!');
        if (!ahead && !behind) {
            return false;
        }
        i_ += behind ? 4 : 3;
        group_body(f);
        return true;
    }

    [[nodiscard]] constexpr bool is_quantifier_start() const noexcept {
        const char32_t c = peek();
        if (c == '*' || c == '+' || c == '?') {
            return true;
        }
        std::size_t at = i_;
        std::size_t least = 0;
        std::size_t most = 0;
        bool bounded = true;
        return c == '{' && braces(at, least, most, bounded);
    }

    constexpr void no_quantifier() noexcept {
        if (failed()) {
            return;
        }
        if (is_quantifier_start()) {
            fail("a quantifier after an assertion");
        } else if (peek() == '{') {
            fail("a lone {");
        }
    }

    // Decimal digits at byte `at`, as a number, moving `at` past them; false when there is none.
    // A number past 2^32 - 1 reads as it: ECMA-262 compares numbers of any size.
    [[nodiscard]] constexpr bool decimal_number(std::size_t& at,
                                                std::size_t& value) const noexcept {
        if (at >= p_.size() || p_[at] < '0' || p_[at] > '9') {
            return false;
        }
        constexpr std::size_t largest = 0xFFFFFFFF;
        value = 0;
        for (; at < p_.size() && p_[at] >= '0' && p_[at] <= '9'; ++at) {
            value = std::min(largest, (value * 10) + static_cast<std::size_t>(p_[at] - '0'));
        }
        return true;
    }

    // `{n}`, `{n,}` or `{n,m}` at byte `at`, moving `at` past it; false when it is none.
    [[nodiscard]] constexpr bool braces(std::size_t& at, std::size_t& least, std::size_t& most,
                                        bool& bounded) const noexcept {
        std::size_t i = at;
        if (i >= p_.size() || p_[i] != '{') {
            return false;
        }
        ++i;
        if (!decimal_number(i, least)) {
            return false;
        }
        most = least;
        bounded = true;
        if (i < p_.size() && p_[i] == ',') {
            ++i;
            bounded = i < p_.size() && p_[i] != '}';
            if (bounded && !decimal_number(i, most)) {
                return false;
            }
        }
        if (i >= p_.size() || p_[i] != '}') {
            return false;
        }
        at = i + 1;
        return true;
    }

    constexpr void quantifier() noexcept {
        const char32_t c = peek();
        if (c == '{') {
            std::size_t least = 0;
            std::size_t most = 0;
            bool bounded = true;
            if (!braces(i_, least, most, bounded)) {
                fail("a lone {");
                return;
            }
            if (bounded && most < least) {
                fail("a {} quantifier's numbers out of order");
                return;
            }
        } else if (c == '*' || c == '+' || c == '?') {
            take();
        } else {
            return;
        }
        eat('?');
        if (is_quantifier_start()) {
            fail("a quantifier with nothing to repeat");
        }
    }

    // Atoms.

    // NOLINTNEXTLINE(misc-no-recursion)
    constexpr bool atom(flags f) noexcept {
        const char32_t c = peek();
        switch (c) {
            case '(': return group(f);
            case '[': return character_class();
            case '\\': take(); return atom_escape();
            case '*':
            case '+':
            case '?': fail("a quantifier with nothing to repeat"); return false;
            case '{':
            case '}':
            case ']': fail("a lone {, } or ]"); return false;
            default: take(); return true;
        }
    }

    // A group's disjunction and its `)`, its opening read.
    // NOLINTNEXTLINE(misc-no-recursion)
    constexpr bool group_body(flags f) noexcept {
        if (!enter()) {
            return false;
        }
        disjunction(f);
        --depth_;
        if (failed()) {
            return false;
        }
        if (!eat(')')) {
            fail("an unterminated group");
            return false;
        }
        return true;
    }

    // NOLINTNEXTLINE(misc-no-recursion)
    constexpr bool group(flags f) noexcept {
        const std::size_t open = i_;
        take();
        if (!eat('?') || eat(':')) {
            return group_body(f);
        }
        if (!eat('<')) {
            regex_flags_seen seen;
            return modifiers(f, seen) && group_body(f);
        }
        if (!group_name()) {
            return false;
        }
        check_duplicate_name(open);
        return !failed() && group_body(f);
    }

    /** The modifiers a group names: each of i, m and s at most once, on either side of `-`. */
    struct regex_flags_seen {
        std::array<bool, 3> flag{};
    };

    // ECMA-262 2025's modifiers before the `:`, `ims-ims`; false, failed, on a repeated or unknown
    // one, or none.
    constexpr bool modifiers(flags& f, regex_flags_seen& seen) noexcept {
        bool any = false;
        bool minus = false;
        while (more() && peek() != ':') {
            const char32_t m = take();
            if (m == '-' && !minus) {
                minus = true;
                continue;
            }
            constexpr std::u32string_view letters = U"ims";
            const std::size_t k = letters.find(m);
            if (k == std::u32string_view::npos || seen.flag[k]) {
                fail("an unknown or repeated modifier, or an unknown group");
                return false;
            }
            seen.flag[k] = true;
            any = true;
            if (k == 0) {
                f.i = !minus;
            }
        }
        if (!any || !eat(':')) {
            fail("an unknown group, or modifiers without a :");
            return false;
        }
        return true;
    }

    // A CharacterEscape other than \c, \0, \x and \u: a control escape or a syntax character,
    // `/`, and in a class `-`.
    [[nodiscard]] static constexpr bool simple_escape(char32_t e, bool in_class) noexcept {
        constexpr std::u32string_view simple = U"fnrtv^$\\.*+?()[]{}|/";
        return simple.find(e) != std::u32string_view::npos || (in_class && e == '-');
    }

    // A CharacterEscape after its first letter `e`, as the code point it names.
    constexpr std::optional<char32_t> character_escape(char32_t e, bool in_class) noexcept {
        if (simple_escape(e, in_class)) {
            return e;
        }
        switch (e) {
            case 'c': {
                const char32_t letter = peek();
                if ((letter < 'a' || letter > 'z') && (letter < 'A' || letter > 'Z')) {
                    fail("a \\c without a letter");
                    return std::nullopt;
                }
                take();
                return static_cast<char32_t>(letter % 32);
            }
            case '0':
                if (peek() >= '0' && peek() <= '9') {
                    fail("an octal escape");
                    return std::nullopt;
                }
                return char32_t{0};
            case 'x': {
                const std::optional<char32_t> c = hex_digits(p_, i_, 2);
                if (!c) {
                    fail("a \\x without two hexadecimal digits");
                }
                return c;
            }
            case 'u': return unicode();
            default: fail("an escape that u mode does not have"); return std::nullopt;
        }
    }

    // Whether `e`, after a backslash, starts a class escape: `\d`, `\D`, `\w`, `\W`, `\s`, `\S`,
    // `\p{...}` or `\P{...}`, which it then reads; failed on an unknown property.
    constexpr bool class_escape(char32_t e) noexcept {
        constexpr std::u32string_view classes = U"dDwWsS";
        if (classes.find(e) != std::u32string_view::npos) {
            return true;
        }
        return (e == 'p' || e == 'P') && property();
    }

    // `\k<name>`, after the k.
    constexpr bool named_backreference() noexcept {
        if (!eat('<')) {
            fail("a \\k without a group name");
            return false;
        }
        const std::size_t name = i_;
        if (!group_name()) {
            return false;
        }
        if (!has_group_named(name)) {
            fail("a \\k to a group that does not exist");
            return false;
        }
        return true;
    }

    // `\N`, its first digit `e` read.
    constexpr bool decimal_backreference(char32_t e) noexcept {
        std::size_t n = e - '0';
        while (peek() >= '0' && peek() <= '9') {
            n = std::min<std::size_t>((n * 10) + (take() - '0'), p_.size() + 1);
        }
        if (n > groups_) {
            fail("a backreference to a group that does not exist");
            return false;
        }
        return true;
    }

    constexpr bool atom_escape() noexcept {
        if (!more()) {
            fail("a trailing backslash");
            return false;
        }
        const char32_t e = take();
        if (class_escape(e)) {
            return true;
        }
        if (failed()) {
            return false;
        }
        if (e == 'k') {
            return named_backreference();
        }
        if (e >= '1' && e <= '9') {
            return decimal_backreference(e);
        }
        return character_escape(e, false).has_value();
    }

    // Classes.

    // One ClassAtom: its code point, or nothing for a class escape; failed on an error.
    constexpr std::optional<char32_t> class_atom() noexcept {
        const char32_t c = take();
        if (c != '\\') {
            return c;
        }
        if (!more()) {
            fail("a trailing backslash");
            return std::nullopt;
        }
        const char32_t e = take();
        if (e == 'b') {
            return char32_t{0x08};
        }
        if (class_escape(e) || failed()) {
            return std::nullopt;
        }
        if ((e >= '1' && e <= '9') || e == 'k' || e == 'B') {
            fail("a backreference, a \\k or a \\B in a class");
            return std::nullopt;
        }
        return character_escape(e, true);
    }

    // One item of a class, or a range of two; false, failed, on an error.
    constexpr bool class_item() noexcept {
        const std::optional<char32_t> from = class_atom();
        if (failed()) {
            return false;
        }
        if (peek() != '-' || peek(1) == ']' || peek(1) == none) {
            return true;
        }
        take();
        const std::optional<char32_t> to = class_atom();
        if (failed()) {
            return false;
        }
        if (!from || !to) {
            fail("a class escape in a range");
            return false;
        }
        if (*from > *to) {
            fail("a range out of order in a class");
            return false;
        }
        return true;
    }

    constexpr bool character_class() noexcept {
        take();
        eat('^');
        while (!eat(']')) {
            if (!more()) {
                fail("an unterminated class");
                return false;
            }
            if (!class_item()) {
                return false;
            }
        }
        return true;
    }
};

/**
 Whether `pattern` is ECMA-262's Pattern[+UnicodeMode], groups nested at most `max_nesting` deep,
 and why when it is not.
*/
constexpr regex_check check_regex(std::string_view pattern, std::size_t max_nesting) noexcept {
    for (std::size_t i = 0; i < pattern.size();) {
        char32_t cp = 0;
        if (!decode_utf8(pattern, i, cp)) {
            return {
                .verdict = regex_verdict::invalid,
                .why = "a pattern that is not well-formed UTF-8",
            };
        }
    }
    return regex_validator(pattern, max_nesting).run();
}

}  // namespace webcpp::lexical::detail

#endif  // WEBCPP_LEXICAL_DETAIL_REGEX_VALIDATOR_HPP
