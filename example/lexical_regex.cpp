// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// ECMA-262 patterns, judged by lexical's syntax validator: each is checked, never compiled or run.

#include <webcpp/lexical/regex.hpp>

#include <iostream>
#include <string_view>

int main() {
    // tag::program[]
    for (const std::string_view pattern : {
             R"(^\p{Letter}+$)",
             R"((?<year>\d{4})-(?<month>\d{2}))",
             R"((?<=\$\d*)[0-9])",
             "^(abc]",
             R"(\Aabc\z)",
             "(?i)abc",
             "(?i:abc)",
         }) {
        std::cout << pattern << " -> " << (webcpp::lexical::is_regex(pattern) ? "valid" : "invalid")
                  << '\n';
    }
    // end::program[]
}
