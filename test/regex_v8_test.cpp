// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Compares is_regex with V8 on every pattern of test/regex/cases/patterns.json, which
// tools/generate_patterns.py writes: V8's verdicts, `new RegExp(pattern, "u")` accepting the
// pattern or throwing a SyntaxError, are test/regex/expected/verdicts.json, which only the oracle
// of test/oracle writes (b2 libs/lexical/test/oracle//update-expected). Every verdict must be
// V8's; each that is not is named, with the pattern, the first ones on the standard error. It runs
// on every target, the two files reaching a WASI program through webcpp.data;
// WEBCPP_TEST_LEXICAL_REGEX names their directory, from test/Jamfile.

#include <webcpp/lexical/regex.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/json.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>

namespace fs = std::filesystem;
namespace json = boost::json;

namespace {

/** The differences shown on the standard error before the rest are only counted. */
constexpr std::size_t shown = 20;

/** The file at `path` parsed as a JSON array, or nothing, said on the standard error. */
std::optional<json::array> read_array(const fs::path& path) {
    const std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << path.string() << ": cannot be opened\n";
        return std::nullopt;
    }
    std::ostringstream text;
    text << in.rdbuf();
    boost::system::error_code error;
    json::value value = json::parse(text.str(), error);
    if (error || !value.is_array()) {
        std::cerr << path.string() << ": not a JSON array\n";
        return std::nullopt;
    }
    return std::move(value.get_array());
}

void every_verdict_is_v8s() {
    const fs::path root(WEBCPP_TEST_LEXICAL_REGEX);
    const std::optional<json::array> patterns = read_array(root / "cases" / "patterns.json");
    const std::optional<json::array> verdicts = read_array(root / "expected" / "verdicts.json");
    if (!BOOST_TEST(patterns.has_value() && verdicts.has_value())) {
        return;
    }
    if (!BOOST_TEST_EQ(patterns->size(), verdicts->size())) {
        return;
    }
    std::size_t differ = 0;
    std::size_t valid = 0;
    for (std::size_t i = 0; i < patterns->size(); ++i) {
        const json::value& pattern = (*patterns)[i];
        const json::value& verdict = (*verdicts)[i];
        if (!BOOST_TEST(pattern.is_string() && verdict.is_int64())) {
            return;
        }
        const json::string& text = pattern.get_string();
        const bool v8 = verdict.get_int64() == 1;
        valid += v8 ? 1 : 0;
        if (webcpp::lexical::is_regex(std::string_view(text.data(), text.size())) == v8) {
            continue;
        }
        if (++differ <= shown) {
            std::cerr << "pattern " << i << ": V8 answers " << (v8 ? "valid" : "invalid")
                      << ", is_regex the other: " << json::serialize(pattern) << '\n';
        }
    }
    BOOST_TEST_EQ(differ, 0U);
    std::cout << patterns->size() << " patterns, " << valid << " valid in V8, " << differ
              << " verdicts of is_regex not V8's\n";
}

}  // namespace

int main() {
    every_verdict_is_v8s();
    return boost::report_errors();
}
