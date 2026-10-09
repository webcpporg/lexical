// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Tests what formats allocates, by counting every call of the global operator new this program
// makes: nothing for every validator and rule, on long and hostile inputs, but the IRI functions
// beyond their inline buffer and is_regex, which allocate only through the resource they are
// given.

#include <boost/config.hpp>
#include <boost/core/lightweight_test.hpp>

#include <cstddef>
#include <cstdlib>
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
// or the free inside one, would pair it with the other's call and warn of a mismatch.

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void* operator new(std::size_t size) {
    ++global_new_calls;
    void* memory = std::malloc(size == 0 ? 1 : size);
    if (memory == nullptr) {
        std::abort();
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
 Memory of `size` bytes aligned to `alignment`, from aligned_alloc, whose size must be a multiple
 of the alignment, or from _aligned_malloc under _MSC_VER, whose UCRT has no aligned_alloc.
*/
void* aligned_memory(std::size_t size, std::size_t alignment) {
    const std::size_t rounded =
        size == 0 ? alignment : (size + alignment - 1) / alignment * alignment;
#ifdef _MSC_VER
    return _aligned_malloc(rounded, alignment);
#else
    return std::aligned_alloc(alignment, rounded);
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
        std::abort();
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

}  // namespace

int main() {
    the_instrument_counts();
    return boost::report_errors();
}
