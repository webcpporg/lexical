// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Runs the JSON-Schema-Test-Suite's format tests against lexical's validators, called directly:
// every file of tests/<dialect>/optional/format/ for draft4 to draft2020-12, and of
// tests/v1/format/. A string test passes when its validator's verdict is the test's "valid". A
// non-string test checks json_schema's rule that a format applies to strings only, so it must
// expect valid, and is counted apart. The run fails on a failure its dialect's known-failure list
// does not name, on a listed test that passes or that the suite does not hold, and on a format
// that maps to no validator. It also fails, by name, on a known failure that names a test no
// validator judges (an unknown format, a non-string test), on a list that names a test twice, on a
// case whose schema holds a keyword besides format, and on a line of the lists below that names a
// format or a dialect the suite does not hold. It runs on every target, the suite and its
// known-failure lists reaching a WASI program through webcpp.data.
// WEBCPP_TEST_JSON_SCHEMA_TEST_SUITE names the suite's root, from
// /webcpp/third_party//json-schema-test-suite, and
// WEBCPP_TEST_LEXICAL_KNOWN_FAILURES the lists' directory, from test/Jamfile. A known failure names
// its file as json_schema's lists do, by its path under the dialect's directory
// (optional/format/uuid.json).

#include <webcpp/lexical.hpp>
#include <webcpp/lexical/regex.hpp>

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
#include <utility>
#include <vector>

namespace lexical = webcpp::lexical;
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
    mapping{"date", [](std::string_view s) { return lexical::is_date(s); }},
    mapping{"date-time", [](std::string_view s) { return lexical::is_date_time(s); }},
    mapping{"duration", [](std::string_view s) { return lexical::is_duration(s); }},
    mapping{"email", [](std::string_view s) { return lexical::is_email(s); }},
    mapping{"hostname", [](std::string_view s) { return lexical::is_hostname(s); }},
    mapping{"idn-email", [](std::string_view s) { return lexical::is_idn_email(s); }},
    mapping{"idn-hostname", [](std::string_view s) { return lexical::is_idn_hostname(s); }},
    mapping{"ipv4", [](std::string_view s) { return lexical::is_ipv4(s); }},
    mapping{"ipv6", [](std::string_view s) { return lexical::is_ipv6(s); }},
    mapping{"iri", [](std::string_view s) { return lexical::is_iri(s); }},
    mapping{"iri-reference", [](std::string_view s) { return lexical::is_iri_reference(s); }},
    mapping{"json-pointer", [](std::string_view s) { return lexical::is_json_pointer(s); }},
    mapping{"regex", [](std::string_view s) { return lexical::is_regex(s); }},
    mapping{
        "relative-json-pointer",
        [](std::string_view s) { return lexical::is_relative_json_pointer(s); },
    },
    mapping{"time", [](std::string_view s) { return lexical::is_time(s); }},
    mapping{"uri", [](std::string_view s) { return lexical::is_uri(s); }},
    mapping{"uri-reference", [](std::string_view s) { return lexical::is_uri_reference(s); }},
    mapping{"uri-template", [](std::string_view s) { return lexical::is_uri_template(s); }},
    mapping{"uuid", [](std::string_view s) { return lexical::is_uuid(s); }},
};

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
                return lexical::is_relative_json_pointer(s, lexical::index_manipulation::refused);
            },
    },
    {
        .format = "relative-json-pointer",
        .dialect = "draft7",
        .valid =
            [](std::string_view s) {
                return lexical::is_relative_json_pointer(s, lexical::index_manipulation::refused);
            },
    },
    {
        .format = "time",
        .dialect = "v1",
        .valid =
            [](std::string_view s) { return lexical::is_time(s, lexical::leap_seconds::refused); },
    },
});

/** Whether `range` holds each format once, in order. */
template <class Range, class Projection>
constexpr bool sorted_and_unique(const Range& range, Projection projection) {
    return std::ranges::adjacent_find(range, std::ranges::greater_equal{}, projection) ==
           std::ranges::end(range);
}

// A change that registers a format adds its line: this catches an unsorted or duplicated one.
static_assert(sorted_and_unique(validators, &mapping::format));

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
    std::size_t non_strings = 0;
    std::size_t skipped = 0;
};

/** A dialect's known failures, and those of them its run met. */
struct known_failures {
    std::set<test_id> listed;
    std::set<test_id> met;
};

/** What one dialect's run reads and finds: its known failures, its counts, the formats it met. */
struct dialect_run {
    known_failures failures;
    tally counted;
    std::set<std::string> formats;
};

/** Where a test is, and the validator its case's format maps to: null when unknown. */
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

/**
 Whether every entry of dialect_validators names a dialect of dialects and a format of validators,
 each wrong one named on the standard error.

 Tip: a misspelled entry would leave its dialect on the default validator without a word, and the
 suite tests no index manipulation, so no failure would show it.
*/
bool policies_name_what_exists() {
    bool sound = true;
    for (const dialect_mapping& m : dialect_validators) {
        const bool dialect_exists =
            std::ranges::find(dialects, m.dialect, &dialect::name) != dialects.end();
        const bool format_exists =
            std::ranges::find(validators, m.format, &mapping::format) != validators.end();
        if (!BOOST_TEST(dialect_exists && format_exists)) {
            std::cerr << "dialect_validators: the entry \"" << m.format << "\" / \"" << m.dialect
                      << "\" names no " << (dialect_exists ? "format of validators" : "dialect")
                      << '\n';
            sound = false;
        }
    }
    return sound;
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
        fs::path(WEBCPP_TEST_LEXICAL_KNOWN_FAILURES) / (std::string(name) + ".json");
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
        const std::string_view file = string_at(*object, "file");
        const std::string_view case_description = string_at(*object, "case");
        const std::string_view test = string_at(*object, "test");
        if (!BOOST_TEST(listed.emplace(file, case_description, test).second)) {
            std::cerr << path.string() << ": the known failure \"" << file << "\" / \""
                      << case_description << "\" / \"" << test << "\" is listed twice\n";
        }
    }
    return listed;
}

/** Says why a test failed, on the standard error. */
void report(const where& at, std::string_view test, std::string_view why) {
    std::cerr << at.dialect << '/' << at.file << ": \"" << at.case_description << "\" / \"" << test
              << "\": " << why << '\n';
}

/** The test `description` of the case `at`, as a known failure names it. */
test_id id_of(const where& at, std::string_view description) {
    return {std::string(at.file), std::string(at.case_description), std::string(description)};
}

/** Fails a known failure that names a test no validator judges, saying why. */
void refuse_listed(const where& at, std::string_view description, std::string_view why,
                   known_failures& failures) {
    const test_id id = id_of(at, description);
    if (!failures.listed.contains(id)) {
        return;
    }
    failures.met.insert(id);
    BOOST_ERROR("a known failure names a test no validator judges");
    report(at, description,
           std::string("its dialect's known-failure list names it, and ") + std::string(why));
}

/** Judges one string test of the case `at` against its validator and the known failures. */
void judge(const where& at, std::string_view description, const json::string& text, bool expected,
           dialect_run& run) {
    ++run.counted.strings;
    const bool verdict = at.valid(std::string_view(text.data(), text.size()));
    const test_id id = id_of(at, description);
    const bool listed = run.failures.listed.contains(id);
    if (listed) {
        run.failures.met.insert(id);
        ++run.counted.known;
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
void run_test(const where& at, const json::object& test, dialect_run& run) {
    const json::value* data = test.if_contains("data");
    const json::value* expected = test.if_contains("valid");
    const std::string_view description = string_at(test, "description");
    if (!BOOST_TEST(data != nullptr && expected != nullptr && expected->is_bool())) {
        report(at, description, "a test without its data and its verdict");
        return;
    }
    if (!data->is_string()) {
        ++run.counted.non_strings;
        if (!BOOST_TEST(expected->get_bool())) {
            report(at, description, "a non-string instance expected invalid");
        }
        refuse_listed(at, description, "it is a non-string test", run.failures);
        return;
    }
    if (at.format == unknown_format) {
        ++run.counted.skipped;
        refuse_listed(at, description, "its format \"unknown\" is skipped", run.failures);
        return;
    }
    judge(at, description, data->get_string(), expected->get_bool(), run);
}

/**
 Whether the schema of the case `at` holds no keyword but `format` and `$schema`, each other named
 on the standard error.

 Tip: the runner judges a test on its format alone, so a keyword that changes a verdict would be
 misread, not ignored.
*/
bool schema_is_format_alone(const where& at, const json::object& schema) {
    bool alone = true;
    for (const json::key_value_pair& member : schema) {
        if (!BOOST_TEST(member.key() == "format" || member.key() == "$schema")) {
            std::cerr << at.dialect << '/' << at.file << ": \"" << at.case_description
                      << "\": the schema's keyword \"" << member.key()
                      << "\", which the runner does not judge\n";
            alone = false;
        }
    }
    return alone;
}

/** Runs every test of one case, `group`, of `file` in the dialect `name`. */
void run_case(std::string_view name, std::string_view file, const json::value& group,
              dialect_run& run) {
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
    run.formats.emplace(format);
    if (!BOOST_TEST(at.valid != nullptr || format == unknown_format)) {
        std::cerr << name << '/' << file << ": the format \"" << format
                  << "\" maps to no validator\n";
        return;
    }
    if (!schema_is_format_alone(at, schema->get_object())) {
        return;
    }
    for (const json::value& test : tests->get_array()) {
        if (const json::object* t = test.if_object(); BOOST_TEST(t != nullptr)) {
            run_test(at, *t, run);
        }
    }
}

/** Runs every case of the file at `path`. */
void run_file(const dialect& d, const fs::path& path, dialect_run& run) {
    // The file as json_schema's known-failure lists name it: under the dialect's directory.
    const std::string file = std::string(d.formats) + "/" + path.filename().string();
    const std::optional<json::value> document = read_json(path);
    if (!BOOST_TEST(document.has_value() && document->is_array())) {
        return;
    }
    for (const json::value& group : document->get_array()) {
        run_case(d.name, file, group, run);
    }
}

/** Runs every format test of the dialect `d`, prints what it counted, and returns its formats. */
std::set<std::string> run_dialect(const dialect& d) {
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
        return {};
    }
    std::ranges::sort(files);
    dialect_run run;
    run.failures.listed = read_known_failures(d.name);
    for (const fs::path& path : files) {
        run_file(d, path, run);
    }
    for (const test_id& id : run.failures.listed) {
        if (!BOOST_TEST(run.failures.met.contains(id))) {
            std::cerr << d.name << ": the known failure \"" << std::get<0>(id) << "\" / \""
                      << std::get<1>(id) << "\" / \"" << std::get<2>(id)
                      << "\" is no test of the suite\n";
        }
    }
    const tally& counted = run.counted;
    std::cout << d.name << ": " << counted.strings << " string tests, " << counted.known
              << " known failures, " << counted.non_strings << " non-string, " << counted.skipped
              << " skipped (unknown)\n";
    return std::move(run.formats);
}

/**
 Checks that every format validators and dialect_validators name is one the suite holds,
 in the dialect a policy names: a misspelled line would otherwise judge nothing, without a word.
*/
void check_every_format_met(const std::set<std::pair<std::string, std::string>>& met) {
    const auto anywhere = [&](std::string_view format) {
        return std::ranges::any_of(met, [&](const auto& m) { return m.second == format; });
    };
    for (const mapping& m : validators) {
        if (!BOOST_TEST(anywhere(m.format))) {
            std::cerr << "validators: \"" << m.format
                      << "\" is a format no file of the suite holds\n";
        }
    }
    for (const dialect_mapping& m : dialect_validators) {
        if (!BOOST_TEST(met.contains({std::string(m.dialect), std::string(m.format)}))) {
            std::cerr << "dialect_validators: no file of " << m.dialect << " holds \"" << m.format
                      << "\"\n";
        }
    }
}

}  // namespace

int main() {
    if (!policies_name_what_exists()) {
        return boost::report_errors();
    }
    std::set<std::pair<std::string, std::string>> met;
    for (const dialect& d : dialects) {
        for (std::string format : run_dialect(d)) {
            met.emplace(std::string(d.name), std::move(format));
        }
    }
    check_every_format_met(met);
    return boost::report_errors();
}
