// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// RFC 3987's IRI, checked and mapped to a URI for Boost.URL: where an IRI ends inside a larger
// grammar, which code points it may hold where (2.2, 4.1), and its mapping (3.1 step 2), written
// into storage that is none for ASCII, an inline buffer up to 1,024 octets, and one buffer of a
// memory resource beyond.

#ifndef WEBCPP_LEXICAL_DETAIL_IRI_MAPPING_HPP
#define WEBCPP_LEXICAL_DETAIL_IRI_MAPPING_HPP

#include <webcpp/lexical/config.hpp>

#include <webcpp/lexical/detail/charsets.hpp>
#include <webcpp/lexical/detail/utf8.hpp>

#include <boost/scope/scope_exit.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory_resource>
#include <optional>
#include <span>
#include <string_view>

namespace webcpp::lexical::detail {

/**
 The mapped octets an IRI takes on the stack: any IRI of up to 341 octets, since mapping at most
 triples a length.
*/
inline constexpr std::size_t iri_inline_capacity = 1024;

/**
 Whether `byte` is one no IRI holds: an ASCII control, the space, the double quote, the less-than
 and greater-than signs, the backslash, the circumflex, the grave accent, a brace or the vertical
 bar.
*/
constexpr bool ends_iri(char byte) noexcept {
    const auto b = static_cast<unsigned char>(byte);
    return b <= 0x20U || b == 0x7FU || byte == '"' || byte == '<' || byte == '>' || byte == '\\' ||
           byte == '^' || byte == '`' || byte == '{' || byte == '|' || byte == '}';
}

/** Where the IRI that starts at `first` ends: at the first byte no IRI holds, or at `end`. */
constexpr char const* scan_iri(char const* first, char const* end) noexcept {
    char const* last = first;
    while (last != end && !ends_iri(*last)) {
        ++last;
    }
    return last;
}

/**
 Where an IRI's query, fragment and IP literal lie, as offsets into it, each npos when absent: the
 parts RFC 3986 appendix B splits a reference into, which a valid IRI never mistakes.
*/
struct iri_parts {
    /** The first "#": the fragment follows it. */
    std::size_t fragment = std::string_view::npos;
    /** The first "?": the query follows it, when it precedes the fragment. */
    std::size_t query = std::string_view::npos;
    /** The "[" that opens an IP literal in the authority. */
    std::size_t literal = std::string_view::npos;
    /** Where the authority that holds the IP literal ends. */
    std::size_t authority_end = std::string_view::npos;
};

/** Finds the query, the fragment and the IP literal of `run` (RFC 3986 appendix B). */
constexpr iri_parts find_iri_parts(std::string_view run) noexcept {
    iri_parts parts;
    parts.fragment = run.find('#');
    parts.query = run.find('?');
    // A scheme ends at a colon that comes before any "/", "?" or "#"; an authority starts with "//"
    // after it and runs to the next "/", "?" or "#". Only there may a "[" open an IP literal.
    const std::size_t delimiter = run.find_first_of(":/?#");
    const std::size_t hier =
        delimiter != std::string_view::npos && run[delimiter] == ':' ? delimiter + 1 : 0;
    if (run.size() - hier < 2 || run[hier] != '/' || run[hier + 1] != '/') {
        return parts;
    }
    const std::size_t authority = hier + 2;
    const std::size_t authority_end = std::min(run.find_first_of("/?#", authority), run.size());
    const std::size_t literal = run.find('[', authority);
    if (literal < authority_end) {
        parts.literal = literal;
        parts.authority_end = authority_end;
    }
    return parts;
}

/**
 The length of `run` mapped to a URI, or nothing when it holds ill-formed UTF-8, a bidirectional
 formatting character, a non-ASCII code point inside an IP literal, or one that is not ucschar
 (nor iprivate in the query, the text after the first "?" that precedes the first "#").
*/
constexpr std::optional<std::size_t> mapped_length(std::string_view run) noexcept {
    // No valid IRI holds a "?" before its query or a "#" before its fragment (RFC 3986
    // appendix B): the query is what lies after the first "?" and before the first "#".
    const iri_parts parts = find_iri_parts(run);
    std::size_t length = 0;
    std::size_t i = 0;
    while (i < run.size()) {
        if (static_cast<unsigned char>(run[i]) < 0x80U) {
            ++length;
            ++i;
            continue;
        }
        const std::size_t start = i;
        char32_t cp = 0;
        if (!decode_utf8(run, i, cp) || is_bidi_format(cp)) {
            return std::nullopt;
        }
        // RFC 3987 takes its IP literal from RFC 3986, ASCII alone: a non-ASCII code point there
        // is refused before anything is mapped.
        if (start > parts.literal && start < parts.authority_end) {
            return std::nullopt;
        }
        const bool in_query =
            parts.query != std::string_view::npos && start > parts.query && start < parts.fragment;
        if (!is_ucschar(cp) && !(in_query && is_iprivate(cp))) {
            return std::nullopt;
        }
        length += 3 * (i - start);
    }
    return length;
}

/**
 Writes `run` mapped into `out`, which holds exactly its mapped length: each octet of a non-ASCII
 code point as %XX, uppercase.
*/
constexpr void map_iri(std::string_view run, std::span<char> out) noexcept {
    constexpr std::string_view hex = "0123456789ABCDEF";
    std::size_t o = 0;
    for (const char c : run) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte < 0x80U) {
            out[o++] = c;
            continue;
        }
        out[o++] = '%';
        out[o++] = hex[byte >> 4U];
        out[o++] = hex[byte & 0x0FU];
    }
}

/**
 Checks `run`, maps it, and answers what `parse` answers of the mapped URI: `run` itself when it
 is ASCII, an inline buffer up to iri_inline_capacity, else one buffer from `resource`, or from
 the default resource when it is null.
*/
template <class Parse>
bool with_mapped_iri(std::string_view run, std::pmr::memory_resource* resource, Parse parse) {
    const std::optional<std::size_t> length = mapped_length(run);
    if (!length) {
        return false;
    }
    if (*length == run.size()) {
        return parse(run);
    }
    if (*length <= iri_inline_capacity) {
        // Left uninitialised: map_iri writes every octet that parse reads.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
        std::array<char, iri_inline_capacity> buffer;
        map_iri(run, std::span<char>(buffer.data(), *length));
        return parse(std::string_view(buffer.data(), *length));
    }
    std::pmr::polymorphic_allocator<char> allocator(
        resource != nullptr ? resource : std::pmr::get_default_resource());
    char* const buffer = allocator.allocate(*length);
    const boost::scope::scope_exit release(
        [&]() noexcept { allocator.deallocate(buffer, *length); });
    map_iri(run, std::span<char>(buffer, *length));
    return parse(std::string_view(buffer, *length));
}

}  // namespace webcpp::lexical::detail

#endif  // WEBCPP_LEXICAL_DETAIL_IRI_MAPPING_HPP
