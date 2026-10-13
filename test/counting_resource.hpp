// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// A memory resource that counts what goes through it, for the tests that check that lexical
// allocates only through the resource it is given.

#ifndef WEBCPP_TEST_LEXICAL_COUNTING_RESOURCE_HPP
#define WEBCPP_TEST_LEXICAL_COUNTING_RESOURCE_HPP

#include <cstddef>
#include <memory_resource>
#include <new>

namespace webcpp::test {

/**
 A memory resource of this program's own global operator new and delete, the aligned forms.

 Tip: `std::pmr::new_delete_resource()` is not, under MSVC: its runtime library defines it in its
 DLL, whose calls of operator new never reach the replacement a test program defines, so a test
 that counts the global operator new takes its memory, and makes its default resource, from
 this one.
*/
class program_heap final : public std::pmr::memory_resource {
private:
    void* do_allocate(std::size_t size, std::size_t alignment) override {
        return ::operator new(size, std::align_val_t{alignment});
    }

    void do_deallocate(void* memory, std::size_t /*size*/, std::size_t alignment) override {
        ::operator delete(memory, std::align_val_t{alignment});
    }

    [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};

/** The one `program_heap` of the program. */
inline std::pmr::memory_resource& the_program_heap() {
    static program_heap heap;
    return heap;
}

/**
 A memory resource that counts its allocations, deallocations and bytes, and takes its memory
 from `the_program_heap()`, so each allocation is also one call of the global operator new.
*/
class counting_resource final : public std::pmr::memory_resource {
public:
    std::size_t allocations = 0;
    std::size_t deallocations = 0;
    std::size_t bytes = 0;

private:
    void* do_allocate(std::size_t size, std::size_t alignment) override {
        ++allocations;
        bytes += size;
        return the_program_heap().allocate(size, alignment);
    }

    void do_deallocate(void* memory, std::size_t size, std::size_t alignment) override {
        ++deallocations;
        the_program_heap().deallocate(memory, size, alignment);
    }

    [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};

}  // namespace webcpp::test

#endif  // WEBCPP_TEST_LEXICAL_COUNTING_RESOURCE_HPP
