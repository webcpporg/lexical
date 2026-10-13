// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Where an internationalized name ends inside a larger grammar: UTS 46 may map any non-ASCII code
// point, so only ASCII ends a run.

#ifndef WEBCPP_LEXICAL_DETAIL_IDN_SCAN_HPP
#define WEBCPP_LEXICAL_DETAIL_IDN_SCAN_HPP

#include <webcpp/lexical/config.hpp>

#include <webcpp/lexical/detail/charsets.hpp>

namespace webcpp::lexical::detail {

/** What an IDN run is: a whole domain name, or one label of it. */
enum class idn_run {
    /** A domain name: it holds "." and UTS 46's other label separators. */
    domain,
    /** One label: "." and the separators of is_label_separator end it. */
    label,
};

/**
 Whether the three bytes at `p` are U+3002, U+FF0E or U+FF61, which UTS 46 maps to ".".

 Tip: no other code point maps to a sequence that holds "." in idna's tables (Unicode 17.0.0,
 measured over every code point), so a label's run never holds a separator.
*/
constexpr bool is_label_separator(char const* p, char const* end) noexcept {
    if (end - p < 3) {
        return false;
    }
    const auto b0 = static_cast<unsigned char>(p[0]);
    const auto b1 = static_cast<unsigned char>(p[1]);
    const auto b2 = static_cast<unsigned char>(p[2]);
    return (b0 == 0xE3U && b1 == 0x80U && b2 == 0x82U) ||
           (b0 == 0xEFU && b1 == 0xBCU && b2 == 0x8EU) ||
           (b0 == 0xEFU && b1 == 0xBDU && b2 == 0xA1U);
}

/**
 Where the IDN run that starts at `first` ends: at the first ASCII byte outside letters, digits,
 "-" and "."; a label's also at "." and the three separators of is_label_separator.
*/
constexpr char const* scan_idn_run(char const* first, char const* end, idn_run kind) noexcept {
    char const* last = first;
    while (last != end) {
        if (static_cast<unsigned char>(*last) >= 0x80U) {
            if (kind == idn_run::label && is_label_separator(last, end)) {
                break;
            }
            ++last;
            continue;
        }
        const bool ends = *last == '.' ? kind == idn_run::label : !ldh_chars(*last);
        if (ends) {
            break;
        }
        ++last;
    }
    return last;
}

}  // namespace webcpp::lexical::detail

#endif  // WEBCPP_LEXICAL_DETAIL_IDN_SCAN_HPP
