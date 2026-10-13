# lexical

lexical is a header-only C++20 library of validators and Boost.URL grammar
rules for the string formats of JSON Schema, one of the libraries of
[webcpp](https://github.com/webcpporg/webcpp). Each format is a function that
takes a `std::string_view` and answers `bool`, and, but `regex`, a rule of
`webcpp::lexical::grammar` that composes with Boost.URL's combinators. Each is
checked as its RFC or ECMA-262 defines it, against the official
JSON-Schema-Test-Suite, and `regex` against V8. Nothing allocates but an IRI
too long for its inline buffer, through a `std::pmr::memory_resource`.

| Formats | Functions |
| --- | --- |
| `date-time`, `date`, `time`, `duration` | `is_date_time`, `is_date`, `is_time`, `is_duration` |
| `email`, `idn-email` | `is_email`, `is_idn_email` |
| `hostname`, `idn-hostname` | `is_hostname`, `is_idn_hostname` |
| `ipv4`, `ipv6` | `is_ipv4`, `is_ipv6` |
| `uri`, `uri-reference`, `iri`, `iri-reference` | `is_uri`, `is_uri_reference`, `is_iri`, `is_iri_reference` |
| `uri-template` | `is_uri_template` |
| `json-pointer`, `relative-json-pointer` | `is_json_pointer`, `is_relative_json_pointer` |
| `uuid` | `is_uuid` |
| `regex` | `is_regex` |

## A minimal example

This is [example/lexical_quick_start.cpp](example/lexical_quick_start.cpp):

<!-- include::example/lexical_quick_start.cpp[tag=program] -->
```cpp
#include <webcpp/lexical.hpp>

#include <iostream>
#include <string_view>

namespace lexical = webcpp::lexical;

namespace {

void show(std::string_view format, std::string_view text, bool valid) {
    std::cout << format << ' ' << text << ": " << (valid ? "valid" : "invalid") << '\n';
}

}  // namespace

int main() {
    show("date-time", "1985-04-12T23:20:50.52Z", lexical::is_date_time("1985-04-12T23:20:50.52Z"));
    show("date", "2021-02-29", lexical::is_date("2021-02-29"));
    show("email", "joe.bloggs@[IPv6:::1]", lexical::is_email("joe.bloggs@[IPv6:::1]"));
    show("hostname", "xn--nxasmq6b.example", lexical::is_hostname("xn--nxasmq6b.example"));
    show("uri", "//example.com/p", lexical::is_uri("//example.com/p"));
    show("uri-reference", "//example.com/p", lexical::is_uri_reference("//example.com/p"));
    show("json-pointer", "/a~1b", lexical::is_json_pointer("/a~1b"));
}
```

It prints:

<!-- include::example/lexical_quick_start.expected[] -->
```
date-time 1985-04-12T23:20:50.52Z: valid
date 2021-02-29: invalid
email joe.bloggs@[IPv6:::1]: valid
hostname xn--nxasmq6b.example: valid
uri //example.com/p: invalid
uri-reference //example.com/p: valid
json-pointer /a~1b: valid
```

`<webcpp/lexical.hpp>` includes every format but `regex`, whose header
`<webcpp/lexical/regex.hpp>` brings the Unicode tables of ECMA-262's classes.
lexical needs Boost 1.92 or newer (URL, whose compiled part webcpp builds from
Boost's sources, and System), webcpp's configuration, config, and webcpp::idna,
which `/webcpp/lexical//lexical` brings. It runs natively,
on wasm32-wasip2 and wasm32-wasip3, and on emscripten.

## Building and testing

lexical is developed inside the webcpp superproject, as a Boost library is
developed inside Boost:

    git clone --recursive https://github.com/webcpporg/webcpp
    cd webcpp
    b2 libs/lexical/test libs/lexical/example

builds the tests and the examples natively, runs every example and compares
its output with the `.expected` file beside it.
`toolset=clang-wasip2 testing.launcher=wasmtime`,
`toolset=clang-wasip3 testing.launcher=wasmtime` and `toolset=emscripten`
do the same for WebAssembly. `b2 -a libs/lexical/test/oracle//oracle`
compares the verdicts of V8 with the committed ones, and
`b2 -a libs/lexical/test//generated` checks the generated files against
their generators.

## Documentation

lexical's page, with its API reference, is published at
<https://webcpporg.github.io/libs/lexical/>; `b2 libs/lexical/doc` builds it
into `doc/html/index.html`.

## Unicode data

The tables of lexical's regular expressions and the patterns of its tests are
derived from Unicode's data files, under the Unicode License v3, whose notice
is the appendix "Unicode data" of lexical's page. lexical's code is under the
Boost Software License alone.

## License

Distributed under the [Boost Software License, Version 1.0](LICENSE_1_0.txt).
