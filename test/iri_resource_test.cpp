// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// A resource that refuses to allocate makes is_iri and is_iri_reference throw what it throws,
// never answer a verdict without their mapped copy. Built with exceptions whatever the build asks:
// it catches on purpose.

#include <webcpp/lexical/uri.hpp>

#include <boost/core/lightweight_test.hpp>

#include <memory_resource>
#include <new>
#include <string>

namespace lexical = webcpp::lexical;

int main() {
    std::string beyond = "http://a/";
    for (int i = 0; i < 200; ++i) {
        beyond += "\xC3\xA9";
    }
    bool thrown = false;
    try {
        static_cast<void>(lexical::is_iri(beyond, *std::pmr::null_memory_resource()));
    } catch (const std::bad_alloc&) {
        thrown = true;
    }
    BOOST_TEST(thrown);
    bool reference_thrown = false;
    try {
        static_cast<void>(lexical::is_iri_reference(beyond, *std::pmr::null_memory_resource()));
    } catch (const std::bad_alloc&) {
        reference_thrown = true;
    }
    BOOST_TEST(reference_thrown);
    return boost::report_errors();
}
