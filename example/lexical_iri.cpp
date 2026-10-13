// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// IRIs with a memory resource of the program's own: a long IRI's mapped copy is taken from it, a
// short one stays on the stack.

#include <webcpp/lexical.hpp>

#include <array>
#include <cstddef>
#include <iostream>
#include <memory_resource>
#include <string>

namespace lexical = webcpp::lexical;

int main() {
    // tag::resource[]
    std::array<std::byte, 4096> storage{};
    std::pmr::monotonic_buffer_resource resource(storage.data(), storage.size(),
                                                 std::pmr::null_memory_resource());
    std::string long_iri = "http://example.com/";
    for (int i = 0; i < 300; ++i) {
        long_iri += "\303\251";  // 300 e with an acute accent: 1,819 octets mapped, past 1,024
    }
    std::cout << std::boolalpha;
    std::cout << "short: "
              << lexical::is_iri("http://\306\222\303\270\303\270.example/\317\200", resource)
              << '\n';
    std::cout << "long: " << lexical::is_iri(long_iri, resource) << '\n';
    std::cout << "bidi: " << lexical::is_iri("http://a/\342\200\216", resource) << '\n';
    // end::resource[]
}
