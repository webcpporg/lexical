// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Runs the JSON-Schema-Test-Suite's format tests against formats' validators, called directly:
// every file of tests/<dialect>/optional/format/ for draft4 to draft2020-12, and of
// tests/v1/format/. A string test passes when its validator's verdict is the test's "valid". A
// non-string test checks json_schema's rule that a format applies to strings only, so it must
// expect valid, and is counted apart. The run fails on a failure its dialect's known-failure list
// does not name, on a listed test that passes or that the suite does not hold, and on a format
// that maps to no validator. WEBCPP_TEST_JSON_SCHEMA_TEST_SUITE names the suite's root, from
// /webcpp/third_party//json-schema-test-suite, and WEBCPP_TEST_FORMATS_KNOWN_FAILURES the lists'
// directory, from test/Jamfile. A known failure names its file as json_schema's lists do, by its
// path under the dialect's directory (optional/format/uuid.json).

#include <webcpp/formats.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/json.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <vector>

namespace formats = webcpp::formats;
namespace fs = std::filesystem;
namespace json = boost::json;

namespace {

/** A validator the suite's string tests run against. */
using validator = bool (*)(std::string_view);

/** A format the suite names, and the validator its string tests run against. */
struct mapping {
    /** Keeps each line of validators `mapping{"<format>", <validator>}`, as a format adds it. */
    constexpr mapping(std::string_view format, validator valid) : format(format), valid(valid) {}

    std::string_view format;
    validator valid;
};

/** The validators, one per format the suite names, sorted by format. */
constexpr std::array validators{
    mapping{"date", [](std::string_view s) { return formats::is_date(s); }},
    mapping{"date-time", [](std::string_view s) { return formats::is_date_time(s); }},
    mapping{"duration", [](std::string_view s) { return formats::is_duration(s); }},
    mapping{"json-pointer", [](std::string_view s) { return formats::is_json_pointer(s); }},
    mapping{
        "relative-json-pointer",
        [](std::string_view s) { return formats::is_relative_json_pointer(s); },
    },
    mapping{"time", [](std::string_view s) { return formats::is_time(s); }},
    mapping{"uuid", [](std::string_view s) { return formats::is_uuid(s); }},
};

/**
 The formats the suite names whose validators are not written yet: their string tests are counted
 and skipped by name, never silently. A change that writes a validator moves its format from here
 to validators; the list leaves the runner once it is empty.
*/
constexpr auto pending = std::to_array<std::string_view>({
    "email",
    "hostname",
    "idn-email",
    "idn-hostname",
    "ipv4",
    "ipv6",
    "iri",
    "iri-reference",
    "regex",
    "uri",
    "uri-reference",
    "uri-template",
});

/** A format whose validator depends on the dialect, by the policy that dialect's text cites. */
struct dialect_mapping {
    std::string_view format;
    std::string_view dialect;
    validator valid;
};

/**
 The validators that take a dialect's policy in place of the default one of validators: v1's time
 refuses leap seconds; drafts 7 and 2019-09, which cite draft-handrews-relative-json-pointer-01,
 refuse an index manipulation.
*/
constexpr auto dialect_validators = std::to_array<dialect_mapping>({
    {
        .format = "relative-json-pointer",
        .dialect = "draft2019-09",
        .valid =
            [](std::string_view s) {
                return formats::is_relative_json_pointer(s, formats::index_manipulation::refused);
            },
    },
    {
        .format = "relative-json-pointer",
        .dialect = "draft7",
        .valid =
            [](std::string_view s) {
                return formats::is_relative_json_pointer(s, formats::index_manipulation::refused);
            },
    },
    {
        .format = "time",
        .dialect = "v1",
        .valid =
            [](std::string_view s) { return formats::is_time(s, formats::leap_seconds::refused); },
    },
});

/** Whether `range` holds each format once, in order. */
template <class Range, class Projection>
constexpr bool sorted_and_unique(const Range& range, Projection projection) {
    return std::ranges::adjacent_find(range, std::ranges::greater_equal{}, projection) ==
           std::ranges::end(range);
}

/** Whether no format is both mapped to a validator and pending. */
constexpr bool none_mapped_and_pending() {
    return std::ranges::none_of(validators, [](const mapping& m) {
        return std::ranges::find(pending, m.format) != pending.end();
    });
}

// A change that registers a format edits both lists: these catch an unsorted or duplicated line,
// and a format left pending once its validator is mapped.
static_assert(sorted_and_unique(validators, &mapping::format));
static_assert(sorted_and_unique(pending, std::identity{}));
static_assert(none_mapped_and_pending());

/** The format the suite holds for json_schema's question, an unknown format: skipped by name. */
constexpr std::string_view unknown_format = "unknown";

/** A dialect of the suite, its directory under tests/, and its format tests' under that one. */
struct dialect {
    std::string_view name;
    std::string_view formats;
};

constexpr auto dialects = std::to_array<dialect>({
    {.name = "draft4", .formats = "optional/format"},
    {.name = "draft6", .formats = "optional/format"},
    {.name = "draft7", .formats = "optional/format"},
    {.name = "draft2019-09", .formats = "optional/format"},
    {.name = "draft2020-12", .formats = "optional/format"},
    {.name = "v1", .formats = "format"},
});

/** A test of the suite: its file, its case's description and its own description. */
using test_id = std::tuple<std::string, std::string, std::string>;

/** What a dialect's run counted. */
struct tally {
    std::size_t strings = 0;
    std::size_t known = 0;
    std::size_t pending = 0;
    std::size_t non_strings = 0;
    std::size_t skipped = 0;
};

/** A dialect's known failures, and those of them its run met. */
struct known_failures {
    std::set<test_id> listed;
    std::set<test_id> met;
};

/** Where a test is, and the validator its case's format maps to: null when pending or unknown. */
struct where {
    std::string_view dialect;
    std::string_view file;
    std::string_view case_description;
    std::string_view format;
    validator valid = nullptr;
};

/** The file at `path` parsed as JSON, or nothing, said on the standard error. */
std::optional<json::value> read_json(const fs::path& path) {
    const std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << path.string() << ": cannot be opened\n";
        return std::nullopt;
    }
    std::ostringstream text;
    text << in.rdbuf();
    boost::system::error_code error;
    json::value value = json::parse(text.str(), error);
    if (error) {
        std::cerr << path.string() << ": not JSON: " << error.message() << '\n';
        return std::nullopt;
    }
    return value;
}

/** The string at `key` of `object`, or an empty view when it holds no string there. */
std::string_view string_at(const json::object& object, std::string_view key) {
    const json::value* value = object.if_contains(key);
    if (value == nullptr || !value->is_string()) {
        return {};
    }
    const json::string& s = value->get_string();
    return {s.data(), s.size()};
}

/** The validator of `format` in the dialect `name`, its policy's where it has one. */
validator validator_for(std::string_view format, std::string_view name) {
    for (const dialect_mapping& m : dialect_validators) {
        if (m.format == format && m.dialect == name) {
            return m.valid;
        }
    }
    for (const mapping& m : validators) {
        if (m.format == format) {
            return m.valid;
        }
    }
    return nullptr;
}

/** The known failures the list of the dialect `name` holds; a malformed list fails the run. */
std::set<test_id> read_known_failures(std::string_view name) {
    const fs::path path =
        fs::path(WEBCPP_TEST_FORMATS_KNOWN_FAILURES) / (std::string(name) + ".json");
    std::set<test_id> listed;
    const std::optional<json::value> list = read_json(path);
    if (!BOOST_TEST(list.has_value() && list->is_array())) {
        std::cerr << path.string() << ": no array of known failures\n";
        return listed;
    }
    for (const json::value& entry : list->get_array()) {
        const json::object* object = entry.if_object();
        if (!BOOST_TEST(object != nullptr && !string_at(*object, "file").empty() &&
                        !string_at(*object, "case").empty() &&
                        !string_at(*object, "test").empty() &&
                        !string_at(*object, "reason").empty())) {
            std::cerr << path.string() << ": an entry without its file, case, test and reason\n";
            continue;
        }
        listed.emplace(std::string(string_at(*object, "file")),
                       std::string(string_at(*object, "case")),
                       std::string(string_at(*object, "test")));
    }
    return listed;
}

/** Says why a test failed, on the standard error. */
void report(const where& at, std::string_view test, std::string_view why) {
    std::cerr << at.dialect << '/' << at.file << ": \"" << at.case_description << "\" / \"" << test
              << "\": " << why << '\n';
}

/** Judges one string test of the case `at` against its validator and the known failures. */
void judge(const where& at, std::string_view description, const json::string& text, bool expected,
           known_failures& failures, tally& counted) {
    ++counted.strings;
    const bool verdict = at.valid(std::string_view(text.data(), text.size()));
    const test_id id{std::string(at.file), std::string(at.case_description),
                     std::string(description)};
    const bool listed = failures.listed.contains(id);
    if (listed) {
        failures.met.insert(id);
        ++counted.known;
    }
    if (verdict == expected) {
        if (!BOOST_TEST(!listed)) {
            report(at, description, "passes, and its dialect's known-failure list names it");
        }
    } else if (!BOOST_TEST(listed)) {
        report(at, description,
               expected ? "expected valid, the validator says invalid"
                        : "expected invalid, the validator says valid");
    }
}

/** Runs one test of the case `at`. */
void run_test(const where& at, const json::object& test, known_failures& failures, tally& counted) {
    const json::value* data = test.if_contains("data");
    const json::value* expected = test.if_contains("valid");
    const std::string_view description = string_at(test, "description");
    if (!BOOST_TEST(data != nullptr && expected != nullptr && expected->is_bool())) {
        report(at, description, "a test without its data and its verdict");
        return;
    }
    if (!data->is_string()) {
        ++counted.non_strings;
        if (!BOOST_TEST(expected->get_bool())) {
            report(at, description, "a non-string instance expected invalid");
        }
        return;
    }
    if (at.format == unknown_format) {
        ++counted.skipped;
        return;
    }
    if (at.valid == nullptr) {
        ++counted.pending;
        return;
    }
    judge(at, description, data->get_string(), expected->get_bool(), failures, counted);
}

/** Runs every test of one case, `group`, of `file` in the dialect `name`. */
void run_case(std::string_view name, std::string_view file, const json::value& group,
              known_failures& failures, tally& counted) {
    const json::object* object = group.if_object();
    const json::value* schema = object == nullptr ? nullptr : object->if_contains("schema");
    const json::value* tests = object == nullptr ? nullptr : object->if_contains("tests");
    if (!BOOST_TEST(schema != nullptr && schema->is_object() && tests != nullptr &&
                    tests->is_array())) {
        std::cerr << name << '/' << file << ": a case without its schema and its tests\n";
        return;
    }
    const std::string_view format = string_at(schema->get_object(), "format");
    const where at{
        .dialect = name,
        .file = file,
        .case_description = string_at(*object, "description"),
        .format = format,
        .valid = validator_for(format, name),
    };
    const bool is_pending = std::ranges::find(pending, format) != pending.end();
    if (!BOOST_TEST(at.valid != nullptr || is_pending || format == unknown_format)) {
        std::cerr << name << '/' << file << ": the format \"" << format
                  << "\" maps to no validator\n";
        return;
    }
    for (const json::value& test : tests->get_array()) {
        if (const json::object* t = test.if_object(); BOOST_TEST(t != nullptr)) {
            run_test(at, *t, failures, counted);
        }
    }
}

/** Runs every case of the file at `path`. */
void run_file(const dialect& d, const fs::path& path, known_failures& failures, tally& counted) {
    // The file as json_schema's known-failure lists name it: under the dialect's directory.
    const std::string file = std::string(d.formats) + "/" + path.filename().string();
    const std::optional<json::value> document = read_json(path);
    if (!BOOST_TEST(document.has_value() && document->is_array())) {
        return;
    }
    for (const json::value& group : document->get_array()) {
        run_case(d.name, file, group, failures, counted);
    }
}

/** Runs every format test of the dialect `d`, and prints what it counted. */
void run_dialect(const dialect& d) {
    const fs::path directory =
        fs::path(WEBCPP_TEST_JSON_SCHEMA_TEST_SUITE "/tests") / d.name / d.formats;
    std::vector<fs::path> files;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator(directory, error)) {
        if (entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    if (!BOOST_TEST(!error && !files.empty())) {
        std::cerr << directory.string() << ": no format tests: " << error.message() << '\n';
        return;
    }
    std::ranges::sort(files);
    known_failures failures{.listed = read_known_failures(d.name), .met = {}};
    tally counted;
    for (const fs::path& path : files) {
        run_file(d, path, failures, counted);
    }
    for (const test_id& id : failures.listed) {
        if (!BOOST_TEST(failures.met.contains(id))) {
            std::cerr << d.name << ": the known failure \"" << std::get<0>(id) << "\" / \""
                      << std::get<1>(id) << "\" / \"" << std::get<2>(id)
                      << "\" is no string test of the suite\n";
        }
    }
    std::cout << d.name << ": " << counted.strings << " string tests, " << counted.known
              << " known failures, " << counted.pending << " pending, " << counted.non_strings
              << " non-string, " << counted.skipped << " skipped (unknown)\n";
}

}  // namespace

int main() {
    for (const dialect& d : dialects) {
        run_dialect(d);
    }
    return boost::report_errors();
}
