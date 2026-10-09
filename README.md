# formats

formats is a header-only C++20 library of validators and Boost.URL grammar
rules for the string formats of JSON Schema, one of the libraries of
[webcpp](https://github.com/webcpporg/webcpp). Each format is a function that
takes a `std::string_view` and answers `bool`, allocating nothing, and a rule
of `webcpp::formats::grammar` that composes with Boost.URL's combinators.

formats is built and tested inside a checkout of the superproject:

    b2 libs/formats/test

Its page is published at <https://webcpporg.github.io/webcpp/libs/formats/>.
It is distributed under the Boost Software License, Version 1.0
([LICENSE_1_0.txt](LICENSE_1_0.txt)).
