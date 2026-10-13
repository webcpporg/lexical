// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// ECMA-262 patterns, read by formats' translator in a pool of the program's own and compiled by
// Boost.Regex.

#include <webcpp/formats/regex.hpp>

#include <iostream>
#include <memory_resource>
#include <string_view>

int main() {
    // tag::program[]
    std::pmr::unsynchronized_pool_resource pool;
    for (const std::string_view pattern : {
             R"(^\p{Letter}+$)",
             R"((?<year>\d{4})-(?<month>\d{2}))",
             R"((?<=\$\d*)[0-9])",
             "^(abc]",
             R"(\Aabc\z)",
             "(?i)abc",
             "(?i:abc)",
         }) {
        std::cout << pattern << " -> "
                  << (webcpp::formats::is_regex(pattern, pool) ? "valid" : "invalid") << '\n';
    }
    // end::program[]
}
