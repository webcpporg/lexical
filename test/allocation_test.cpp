// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Tests what lexical allocates, by counting every call of the global operator new this program
// makes: nothing for every validator and rule, on long and hostile inputs, but the IRI functions
// beyond their inline buffer, which allocate only through the resource they are given. is_regex
// allocates nothing at all, on valid, invalid and hostile patterns alike.

#include <webcpp/lexical/date_time.hpp>
#include <webcpp/lexical/email.hpp>
#include <webcpp/lexical/hostname.hpp>
#include <webcpp/lexical/ip.hpp>
#include <webcpp/lexical/json_pointer.hpp>
#include <webcpp/lexical/regex.hpp>
#include <webcpp/lexical/uri.hpp>
#include <webcpp/lexical/uri_template.hpp>
#include <webcpp/lexical/uuid.hpp>

#include <boost/config.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/throw_exception.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory_resource>
#include <new>
#include <string>
#include <string_view>

#ifdef _MSC_VER
#include <malloc.h>
#endif

#include "counting_resource.hpp"

namespace {

// The calls of the global operator new this program has made, and of operator delete on memory
// it gave. They are mutable global state, which no library holds: this test program's own
// instruments, the one place where the replaced operators below can record what they see. A
// direct call of malloc, calloc or realloc is not counted: interposing them is outside this
// test, and lexical calls none of them.
std::size_t global_new_calls = 0;     // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
std::size_t global_delete_calls = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

/** The calls of the global operator new so far. */
std::size_t global_allocations() {
    return global_new_calls;
}

/** The calls of the global operator delete on memory it was given, so far. */
std::size_t global_deallocations() {
    return global_delete_calls;
}

}  // namespace

// The replacements of the global operator new and operator delete, for this program alone. The
// standard library declares them with parameter names reserved to the implementation, which a
// program may not use, hence the exemption on each. None is ever inlined: GCC, seeing the malloc
// or the free inside one, would pair it with the other's call and warn of a mismatch. Out of
// memory, each raises std::bad_alloc, as the standard asks of a replacement, through
// boost::throw_exception, which aborts in a build without exceptions.

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void* operator new(std::size_t size) {
    ++global_new_calls;
    void* memory = std::malloc(size == 0 ? 1 : size);
    if (memory == nullptr) {
        boost::throw_exception(std::bad_alloc());
    }
    return memory;
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void operator delete(void* memory) noexcept {
    global_delete_calls += memory != nullptr ? 1 : 0;
    std::free(memory);
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void operator delete(void* memory, std::size_t /*size*/) noexcept {
    global_delete_calls += memory != nullptr ? 1 : 0;
    std::free(memory);
}

namespace {

/**
 Memory of `size` bytes aligned to `alignment`, or null: from aligned_alloc, with the alignment
 raised to a pointer's size and the size rounded up to a multiple of it, or from _aligned_malloc
 under _MSC_VER, whose UCRT has no aligned_alloc.

 Tip: macOS's aligned_alloc refuses an alignment below a pointer's size, which libstdc++'s
 new_delete_resource asks for, and libstdc++'s own aligned operator new raises it the same way.
*/
void* aligned_memory(std::size_t size, std::size_t alignment) {
    const std::size_t raised = std::max(alignment, sizeof(void*));
    if (size > std::numeric_limits<std::size_t>::max() - raised) {
        return nullptr;
    }
    const std::size_t rounded = size == 0 ? raised : (size + raised - 1) / raised * raised;
#ifdef _MSC_VER
    return _aligned_malloc(rounded, raised);
#else
    return std::aligned_alloc(raised, rounded);
#endif
}

/** Gives back what aligned_memory took. */
void free_aligned_memory(void* memory) noexcept {
#ifdef _MSC_VER
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}

}  // namespace

// The aligned forms count too: libstdc++'s new_delete_resource allocates through them whatever
// the alignment, where libc++ uses the plain form for the default alignment.

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void* operator new(std::size_t size, std::align_val_t alignment) {
    ++global_new_calls;
    void* memory = aligned_memory(size, static_cast<std::size_t>(alignment));
    if (memory == nullptr) {
        boost::throw_exception(std::bad_alloc());
    }
    return memory;
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void operator delete(void* memory, std::align_val_t /*alignment*/) noexcept {
    global_delete_calls += memory != nullptr ? 1 : 0;
    free_aligned_memory(memory);
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void operator delete(void* memory, std::size_t /*size*/,
                                    std::align_val_t /*alignment*/) noexcept {
    global_delete_calls += memory != nullptr ? 1 : 0;
    free_aligned_memory(memory);
}

namespace {

/** The instrument counts: a string past any small-string buffer, and a resource's allocation. */
void the_instrument_counts() {
    const std::size_t before = global_allocations();
    {
        const std::string long_text(1000, 'x');
        BOOST_TEST_EQ(long_text.size(), 1000U);
    }
    BOOST_TEST_EQ(global_allocations() - before, 1U);
    webcpp::test::counting_resource resource;
    const std::size_t again = global_allocations();
    void* memory = resource.allocate(64, alignof(std::max_align_t));
    resource.deallocate(memory, 64, alignof(std::max_align_t));
    BOOST_TEST_EQ(resource.allocations, 1U);
    BOOST_TEST_EQ(resource.bytes, 64U);
    BOOST_TEST_EQ(global_allocations() - again, 1U);
}

/**
 Every power-of-two alignment from 1 to 64 reaches a replaced operator new once, through the
 resource, and comes back aligned.

 Tip: libstdc++'s new_delete_resource takes the aligned form for every alignment, and libc++ for
 one past the default alignment, so each lane reaches both replacements, below the alignment
 aligned_alloc accepts too.
*/
void every_alignment_is_counted_and_honoured() {
    constexpr std::array<std::size_t, 7> alignments{1, 2, 4, 8, 16, 32, 64};
    for (const std::size_t alignment : alignments) {
        webcpp::test::counting_resource resource;
        const std::size_t before = global_allocations();
        void* memory = resource.allocate(24, alignment);
        BOOST_TEST_EQ(reinterpret_cast<std::uintptr_t>(memory) % alignment, 0U);
        resource.deallocate(memory, 24, alignment);
        BOOST_TEST_EQ(global_allocations() - before, 1U);
    }
}

#ifndef BOOST_NO_EXCEPTIONS
/**
 Each replaced operator new throws std::bad_alloc when it has no memory to give, as the standard
 asks of a replacement: the aligned form also where rounding the size up would overflow.
*/
void exhaustion_throws_bad_alloc() {
    // Read at run time: a constant size would let GCC warn that it exceeds any object's.
    const volatile std::size_t impossible = std::numeric_limits<std::size_t>::max();
    bool plain = false;
    try {
        ::operator delete(::operator new(impossible));
    } catch (const std::bad_alloc&) {
        plain = true;
    }
    BOOST_TEST(plain);
    bool aligned = false;
    try {
        ::operator delete(::operator new(impossible, std::align_val_t{64}), std::align_val_t{64});
    } catch (const std::bad_alloc&) {
        aligned = true;
    }
    BOOST_TEST(aligned);
}
#endif

/**
 Dates, times and durations allocate nothing, a fraction or a number of 100,000 digits
 included.
*/
void dates_and_times_allocate_nothing() {
    std::string fraction = "1985-04-12T23:20:50.";
    fraction.append(100'000, '9');
    fraction += 'Z';
    std::string duration = "P";
    duration.append(100'000, '7');
    duration += 'D';
    const std::size_t before = global_allocations();
    BOOST_TEST(webcpp::lexical::is_date_time(fraction));
    BOOST_TEST(webcpp::lexical::is_duration(duration));
    BOOST_TEST(webcpp::lexical::is_date("2020-02-29"));
    BOOST_TEST(webcpp::lexical::is_time("23:59:60Z"));
    BOOST_TEST(!webcpp::lexical::is_time("23:59:60Z", webcpp::lexical::leap_seconds::refused));
    BOOST_TEST_EQ(global_allocations() - before, 0U);
}

/** UUIDs and JSON Pointers allocate nothing, a pointer of 100,000 tokens included. */
void identifiers_allocate_nothing() {
    std::string pointer;
    for (int i = 0; i < 100'000; ++i) {
        pointer += "/\xC3\xA9~0";
    }
    const std::size_t before = global_allocations();
    BOOST_TEST(webcpp::lexical::is_json_pointer(pointer));
    BOOST_TEST(webcpp::lexical::is_relative_json_pointer("2/highly/nested/objects"));
    BOOST_TEST(webcpp::lexical::is_uuid("2eb8aa08-aa98-11ea-b4aa-73b441d16380"));
    BOOST_TEST_EQ(global_allocations() - before, 0U);
}

/** IP addresses, URIs and URI references allocate nothing, a URI of 100,000 octets included. */
void uris_allocate_nothing() {
    std::string long_uri = "http://example.com/";
    long_uri.append(100'000, 'a');
    const std::size_t before = global_allocations();
    BOOST_TEST(webcpp::lexical::is_ipv4("192.168.0.1"));
    BOOST_TEST(webcpp::lexical::is_ipv6("2001:DB8::8:800:200C:417A"));
    BOOST_TEST(webcpp::lexical::is_uri(long_uri));
    BOOST_TEST(webcpp::lexical::is_uri_reference("//example.com/p"));
    BOOST_TEST(!webcpp::lexical::is_uri_reference("//a@b@example.com/"));
    BOOST_TEST_EQ(global_allocations() - before, 0U);
}

/**
 Host names and e-mail addresses allocate nothing: a 100,000-octet name, and A-labels idna
 checks.
*/
void hosts_allocate_nothing() {
    std::string long_name;
    while (long_name.size() < 100'000) {
        long_name += std::string(63, 'a') + ".";
    }
    long_name += "com";
    const std::string long_email = "a@" + long_name;
    const std::size_t before = global_allocations();
    BOOST_TEST(!webcpp::lexical::is_hostname(long_name));
    BOOST_TEST(webcpp::lexical::is_hostname("xn--9n2bp8q.xn--9t4b11yi5a"));
    BOOST_TEST(!webcpp::lexical::is_hostname("xn--07jceefgh4c"));
    BOOST_TEST(webcpp::lexical::is_email("\"joe bloggs\"@[IPv6:2001:db8::1]"));
    BOOST_TEST(!webcpp::lexical::is_email(long_email));
    BOOST_TEST_EQ(global_allocations() - before, 0U);
}

/**
 IRIs allocate nothing for ASCII and up to 1,024 mapped octets, and beyond them one buffer from the
 resource given, else one call of the global operator new through the default resource, each given
 back; templates allocate nothing, 100,000 expressions included.
*/
void iris_and_templates_allocate_within_their_bounds() {
    std::string within = "a:";
    for (int i = 0; i < 168; ++i) {
        within += "\xC3\xA9";
    }
    within += "\xE2\x82\xAC";
    std::string long_ascii = "http://example.com/";
    long_ascii.append(1'000'000, 'a');
    std::string beyond = "http://a/";
    for (int i = 0; i < 100'000; ++i) {
        beyond += "\xC3\xA9";
    }
    std::string long_template;
    for (int i = 0; i < 100'000; ++i) {
        long_template += "{+v:12}/x";
    }
    webcpp::test::counting_resource resource;
    const std::size_t before = global_allocations();
    const std::size_t released = global_deallocations();
    BOOST_TEST(webcpp::lexical::is_iri(within));
    BOOST_TEST(webcpp::lexical::is_iri(long_ascii));
    BOOST_TEST(webcpp::lexical::is_iri_reference("//\xC6\x92\xC3\xB8\xC3\xB8.example/"));
    BOOST_TEST(webcpp::lexical::is_uri_template(long_template));
    BOOST_TEST_EQ(global_allocations() - before, 0U);
    BOOST_TEST(webcpp::lexical::is_iri(beyond, resource));
    BOOST_TEST_EQ(resource.allocations, 1U);
    BOOST_TEST_EQ(resource.bytes, 9U + 600'000U);
    BOOST_TEST_EQ(resource.deallocations, 1U);
    BOOST_TEST_EQ(global_allocations() - before, 1U);
    BOOST_TEST(webcpp::lexical::is_iri_reference(beyond));
    BOOST_TEST_EQ(global_allocations() - before, 2U);
    // Each buffer is given back, the default resource's as the given one's.
    BOOST_TEST_EQ(global_deallocations() - released, 2U);
}

/**
 Internationalized names and addresses allocate nothing, each call counted alone: idna works in
 fixed storage, and every ASCII form below is longer than any small-string buffer.
*/
void idn_allocates_nothing() {
    std::string long_idn;
    for (int i = 0; i < 4; ++i) {
        long_idn +=
            "b\xC3\xBC"
            "cher.";
    }
    long_idn += "example";  // 63 octets in the ASCII form
    std::string hostile(100'000, 'a');
    hostile += "\xC3\xBC";
    const std::string mailbox = "\xCE\xB4@" + long_idn;
    // A refused domain, after labels already converted.
    const std::string refused = "\xCE\xB4@" + long_idn +
                                ".a\xC2\xB7"
                                "b";
    // 55 "a" and "u" with diaeresis: an A-label of 63 octets.
    const std::string label = std::string(55, 'a') + "\xC3\xBC";
    std::size_t before = global_allocations();
    BOOST_TEST(webcpp::lexical::is_idn_hostname(long_idn));
    BOOST_TEST_EQ(global_allocations() - before, 0U);
    before = global_allocations();
    BOOST_TEST(!webcpp::lexical::is_idn_hostname(hostile));
    BOOST_TEST_EQ(global_allocations() - before, 0U);
    before = global_allocations();
    BOOST_TEST(webcpp::lexical::is_idn_email(mailbox));
    BOOST_TEST_EQ(global_allocations() - before, 0U);
    before = global_allocations();
    BOOST_TEST(!webcpp::lexical::is_idn_email(refused));
    BOOST_TEST_EQ(global_allocations() - before, 0U);
    before = global_allocations();
    char const* it = label.data();
    BOOST_TEST(webcpp::lexical::grammar::idna_label_rule.parse(it, label.data() + label.size())
                   .has_value());
    BOOST_TEST_EQ(global_allocations() - before, 0U);
}

/**
 Every validator answers a hostile megabyte without a crash and without allocating, but through
 the resources it is given: digits, a token, a label and a code point repeated a million times.
 is_regex refuses one past max_regex_length before it reads it, and allocates nothing either.
*/
void every_format_answers_a_megabyte() {
    const std::string digits(std::size_t{1} << 20U, '7');
    const std::string letters(std::size_t{1} << 20U, 'a');
    std::string umlauts;
    for (std::size_t i = 0; i < (std::size_t{1} << 19U); ++i) {
        umlauts += "\303\274";
    }
    std::string braces;
    for (std::size_t i = 0; i < (std::size_t{1} << 17U); ++i) {
        braces += "{a,b}/x";
    }
    webcpp::test::counting_resource resource;
    const std::size_t before = global_allocations();
    namespace lexical = webcpp::lexical;
    BOOST_TEST(!lexical::is_date_time(digits));
    BOOST_TEST(!lexical::is_time(digits));
    BOOST_TEST(!lexical::is_duration(digits));
    BOOST_TEST(!lexical::is_uuid(letters));
    BOOST_TEST(!lexical::is_json_pointer(letters));
    BOOST_TEST(lexical::is_relative_json_pointer(digits));
    BOOST_TEST(!lexical::is_ipv4(digits));
    BOOST_TEST(!lexical::is_ipv6(digits));
    BOOST_TEST(!lexical::is_uri(letters));
    BOOST_TEST(lexical::is_uri_reference(letters));
    BOOST_TEST(lexical::is_iri_reference(umlauts, resource));
    BOOST_TEST(lexical::is_uri_template(braces));
    BOOST_TEST(!lexical::is_hostname(letters));
    BOOST_TEST(!lexical::is_idn_hostname(umlauts));
    BOOST_TEST(!lexical::is_email(letters));
    BOOST_TEST(!lexical::is_idn_email(umlauts));
    BOOST_TEST(!lexical::is_regex(letters));
    BOOST_TEST_EQ(global_allocations() - before, resource.allocations);
}

/** A pattern is_regex answers, and the verdict it is answered with. */
struct regex_sample {
    std::string_view pattern;
    bool valid;
};

/** `levels` groups nested one inside the other around an `a`. */
std::string nested_groups(std::size_t levels) {
    return std::string(levels, '(') + "a" + std::string(levels, ')');
}

/**
 is_regex allocates nothing, on valid patterns, invalid ones and hostile ones: at the nesting and
 length bounds and past them, where it answers false without reading. The patterns are built
 before the count starts.
*/
void a_regex_allocates_nothing() {
    const std::string at_bound = nested_groups(webcpp::lexical::max_regex_nesting);
    const std::string beyond_bound = nested_groups(webcpp::lexical::max_regex_nesting + 1);
    const std::string longest(webcpp::lexical::max_regex_length, 'a');
    const std::string too_long(webcpp::lexical::max_regex_length + 1, 'a');
    const std::string hostile(10'000, '(');
    const std::array samples{
        regex_sample{.pattern = "a", .valid = true},
        regex_sample{.pattern = R"(^\p{Letter}+$)", .valid = true},
        regex_sample{.pattern = R"((?<y>\d{4})-(?<m>\d{2})|\p{Script=Greek}+)", .valid = true},
        regex_sample{.pattern = "(?<=a|bc)d", .valid = true},
        regex_sample{.pattern = R"([\D\S])", .valid = true},
        regex_sample{.pattern = "(?i:k)(?<n>a)|(?<n>b)\\k<n>", .valid = true},
        regex_sample{.pattern = "^(abc]", .valid = false},
        regex_sample{.pattern = "a{3,1}", .valid = false},
        regex_sample{.pattern = "\\k<m>(?<n>a)", .valid = false},
        regex_sample{.pattern = "\\p{Greek}", .valid = false},
        regex_sample{.pattern = at_bound, .valid = true},
        regex_sample{.pattern = beyond_bound, .valid = false},
        regex_sample{.pattern = longest, .valid = true},
        regex_sample{.pattern = too_long, .valid = false},
        regex_sample{.pattern = hostile, .valid = false},
    };
    for (const regex_sample& sample : samples) {
        const std::size_t before = global_allocations();
        const std::size_t freed = global_deallocations();
        BOOST_TEST_EQ(webcpp::lexical::is_regex(sample.pattern), sample.valid);
        BOOST_TEST_EQ(global_allocations() - before, 0U);
        BOOST_TEST_EQ(global_deallocations() - freed, 0U);
    }
}

}  // namespace

int main() {
    the_instrument_counts();
    every_alignment_is_counted_and_honoured();
#ifndef BOOST_NO_EXCEPTIONS
    exhaustion_throws_bad_alloc();
#endif
    dates_and_times_allocate_nothing();
    identifiers_allocate_nothing();
    uris_allocate_nothing();
    hosts_allocate_nothing();
    iris_and_templates_allocate_within_their_bounds();
    idn_allocates_nothing();
    every_format_answers_a_megabyte();
    a_regex_allocates_nothing();
    return boost::report_errors();
}
