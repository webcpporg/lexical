// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Tests what formats allocates, by counting every call of the global operator new this program
// makes: nothing for every validator and rule, on long and hostile inputs, but the IRI functions
// beyond their inline buffer and is_regex, which allocate only through the resource they are
// given.

#include <webcpp/formats/date_time.hpp>
#include <webcpp/formats/json_pointer.hpp>
#include <webcpp/formats/uuid.hpp>

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

#ifdef _MSC_VER
#include <malloc.h>
#endif

#include "counting_resource.hpp"

namespace {

// The calls of the global operator new this program has made. It is mutable global state, which
// no library holds: it is this test program's own instrument, the one place where the replaced
// operator new below can record what it sees.
std::size_t global_new_calls = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

/** The calls of the global operator new so far. */
std::size_t global_allocations() {
    return global_new_calls;
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
    std::free(memory);
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void operator delete(void* memory, std::size_t /*size*/) noexcept {
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
    free_aligned_memory(memory);
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void operator delete(void* memory, std::size_t /*size*/,
                                    std::align_val_t /*alignment*/) noexcept {
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
    BOOST_TEST(webcpp::formats::is_date_time(fraction));
    BOOST_TEST(webcpp::formats::is_duration(duration));
    BOOST_TEST(webcpp::formats::is_date("2020-02-29"));
    BOOST_TEST(webcpp::formats::is_time("23:59:60Z"));
    BOOST_TEST(!webcpp::formats::is_time("23:59:60Z", webcpp::formats::leap_seconds::refused));
    BOOST_TEST_EQ(global_allocations() - before, 0U);
}

/** UUIDs and JSON Pointers allocate nothing, a pointer of 100,000 tokens included. */
void identifiers_allocate_nothing() {
    std::string pointer;
    for (int i = 0; i < 100'000; ++i) {
        pointer += "/\xC3\xA9~0";
    }
    const std::size_t before = global_allocations();
    BOOST_TEST(webcpp::formats::is_json_pointer(pointer));
    BOOST_TEST(webcpp::formats::is_relative_json_pointer("2/highly/nested/objects"));
    BOOST_TEST(webcpp::formats::is_uuid("2eb8aa08-aa98-11ea-b4aa-73b441d16380"));
    BOOST_TEST_EQ(global_allocations() - before, 0U);
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
    return boost::report_errors();
}
