// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// lexical's rules composed with Boost.URL's combinators: an internationalized host and an optional
// port, and who refused what.

#include <webcpp/lexical.hpp>

#include <boost/url/grammar/delim_rule.hpp>
#include <boost/url/grammar/optional_rule.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/tuple_rule.hpp>
#include <boost/url/grammar/unsigned_rule.hpp>

#include <boost/system/error_code.hpp>

#include <cstdint>
#include <iostream>
#include <string_view>

namespace urlg = boost::urls::grammar;
namespace fg = webcpp::lexical::grammar;

int main() {
    // tag::host_port[]
    constexpr auto idn_host_port = urlg::tuple_rule(
        fg::idn_hostname_rule,
        urlg::optional_rule(urlg::tuple_rule(urlg::squelch(urlg::delim_rule(':')),
                                             urlg::unsigned_rule<std::uint16_t>{})));

    for (const std::string_view text : {
             "b\303\274cher.example:8080",
             "example.com",
             "a\302\267b.example:80",
             "example.com:99999",
         }) {
        const auto parsed = urlg::parse(text, idn_host_port);
        if (parsed.has_error()) {
            const boost::system::error_code error = parsed.error();
            std::cout << text << " -> refused by " << error.category().name() << '\n';
            continue;
        }
        const auto& [host, port] = *parsed;
        std::cout << text << " -> host " << host;
        if (port) {
            std::cout << ", port " << *port;
        }
        std::cout << '\n';
    }
    // end::host_port[]
}
