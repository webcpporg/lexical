// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// A few validators, each a function of a std::string_view that answers bool.

// tag::program[]
#include <webcpp/formats.hpp>

#include <iostream>
#include <string_view>

namespace formats = webcpp::formats;

namespace {

void show(std::string_view format, std::string_view text, bool valid) {
    std::cout << format << ' ' << text << ": " << (valid ? "valid" : "invalid") << '\n';
}

}  // namespace

int main() {
    show("date-time", "1985-04-12T23:20:50.52Z", formats::is_date_time("1985-04-12T23:20:50.52Z"));
    show("date", "2021-02-29", formats::is_date("2021-02-29"));
    show("email", "joe.bloggs@[IPv6:::1]", formats::is_email("joe.bloggs@[IPv6:::1]"));
    show("hostname", "xn--nxasmq6b.example", formats::is_hostname("xn--nxasmq6b.example"));
    show("uri", "//example.com/p", formats::is_uri("//example.com/p"));
    show("uri-reference", "//example.com/p", formats::is_uri_reference("//example.com/p"));
    show("json-pointer", "/a~1b", formats::is_json_pointer("/a~1b"));
}

// end::program[]
