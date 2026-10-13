// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// lexical's rules composed with Boost.URL's combinators: every public rule inside tuple_rule,
// variant_rule, optional_rule and range_rule; where each stops; a rule that fails as a whole
// rather than match a prefix; the host-and-port example of the page; and an IDN rule's error,
// which is idna's.

#include <webcpp/lexical.hpp>

#include <webcpp/idna/errors.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/url/grammar/delim_rule.hpp>
#include <boost/url/grammar/optional_rule.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/range_rule.hpp>
#include <boost/url/grammar/tuple_rule.hpp>
#include <boost/url/grammar/unsigned_rule.hpp>
#include <boost/url/grammar/variant_rule.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <tuple>

namespace fg = webcpp::lexical::grammar;
namespace idna = webcpp::idna;
namespace urlg = boost::urls::grammar;

namespace {

/** A comma, then an internationalized host name: the rest of a list of them. */
constexpr auto next_idn_host =
    urlg::tuple_rule(urlg::squelch(urlg::delim_rule(',')), fg::idn_hostname_rule);

/** A dot, then an internationalized label. */
constexpr auto next_label =
    urlg::tuple_rule(urlg::squelch(urlg::delim_rule('.')), fg::idna_label_rule);

/** A space, then a host name. */
constexpr auto next_host =
    urlg::tuple_rule(urlg::squelch(urlg::delim_rule(' ')), fg::hostname_rule);

/** Checks that `rule` matches `matched`, a prefix of `text`, and stops there. */
template <class Rule>
void stops(const Rule& rule, std::string_view text, std::string_view matched) {
    char const* it = text.data();
    const auto r = rule.parse(it, text.data() + text.size());
    if (!BOOST_TEST(r.has_value())) {
        std::cerr << "no match: \"" << text << "\"\n";
        return;
    }
    BOOST_TEST_EQ(text.substr(0, static_cast<std::size_t>(it - text.data())), matched);
}

/** Checks that `rule` refuses `text` whole, rather than match a prefix, and leaves its input. */
template <class Rule>
void fails_whole(const Rule& rule, std::string_view text) {
    char const* it = text.data();
    const auto r = rule.parse(it, text.data() + text.size());
    if (!BOOST_TEST(r.has_error())) {
        std::cerr << "matched a prefix of: \"" << text << "\"\n";
    }
    BOOST_TEST(it == text.data());
}

void where_each_rule_stops() {
    stops(fg::date_time_rule, "2020-01-01T00:00:00Z/x", "2020-01-01T00:00:00Z");
    stops(fg::uuid_rule, "00000000-0000-0000-0000-000000000000x",
          "00000000-0000-0000-0000-000000000000");
    stops(fg::hostname_rule, "ex.ample_x", "ex.ample");
    stops(fg::idn_hostname_rule,
          "b\xC3\xBC"
          "cher.example/x",
          "b\xC3\xBC"
          "cher.example");
    stops(fg::idna_label_rule,
          "b\xC3\xBC"
          "cher.example",
          "b\xC3\xBC"
          "cher");
    stops(fg::idna_label_rule,
          "a\xE3\x80\x82"
          "b",
          "a");
    stops(fg::idna_label_rule,
          "a\xEF\xBC\x8E"
          "b",
          "a");
    stops(fg::idna_label_rule,
          "a\xEF\xBD\xA1"
          "b",
          "a");
    // A separator at the input's end, where the bytes after it are fewer than its own.
    stops(fg::idna_label_rule, "example\xE3\x80\x82", "example");
    stops(fg::uri_rule, "http://a/b c", "http://a/b");
    stops(fg::iri_rule, "http://a/\xC3\xA9>x", "http://a/\xC3\xA9");
    stops(fg::email_rule, "a@b.example,c@d.example", "a@b.example");
    stops(fg::idn_email_rule,
          "\xCE\xB4@b\xC3\xBC"
          "cher.example x",
          "\xCE\xB4@b\xC3\xBC"
          "cher.example");
    stops(fg::uri_template_rule, "{a}/b c", "{a}/b");
}

void runs_fail_whole() {
    fails_whole(fg::hostname_rule, "a..b:80");
    fails_whole(fg::idn_hostname_rule,
                "a\xC2\xB7"
                "b/x");
    fails_whole(fg::iri_rule, "http://a/\xE2\x80\x8E x");
}

void rules_inside_the_combinators() {
    const auto either = urlg::variant_rule(fg::date_time_rule, fg::date_rule, fg::time_rule);
    const auto date = urlg::parse("2020-01-01", either);
    BOOST_TEST(date.has_value() && date->index() == 1);
    const auto time = urlg::parse("12:00:00Z", either);
    BOOST_TEST(time.has_value() && time->index() == 2);
    const auto maybe = urlg::parse("", urlg::optional_rule(fg::uuid_rule));
    BOOST_TEST(maybe.has_value() && !maybe->has_value());
    const auto hosts = urlg::parse(
        "a.example,b\xC3\xBC"
        "cher.example,c.example",
        urlg::range_rule(urlg::ref(fg::idn_hostname_rule), urlg::ref(next_idn_host), 1));
    BOOST_TEST(hosts.has_value() && hosts->size() == 3);
    const auto ascii_hosts =
        urlg::parse("a.example b.example",
                    urlg::range_rule(urlg::ref(fg::hostname_rule), urlg::ref(next_host), 1));
    BOOST_TEST(ascii_hosts.has_value() && ascii_hosts->size() == 2);
    const auto refused_leap = urlg::parse(
        "23:59:60Z",
        urlg::variant_rule(fg::time_rule_t{webcpp::lexical::leap_seconds::refused}, fg::date_rule));
    BOOST_TEST(!refused_leap.has_value());
    const auto host_or_reference = urlg::parse(
        "../a b", urlg::tuple_rule(urlg::optional_rule(fg::hostname_rule), fg::iri_reference_rule,
                                   urlg::squelch(urlg::delim_rule(' ')), fg::hostname_rule));
    BOOST_TEST(host_or_reference.has_value());
    const auto mailboxes = urlg::parse(
        "a@b.example;\"c d\"@[127.0.0.1]",
        urlg::tuple_rule(fg::email_rule, urlg::squelch(urlg::delim_rule(';')), fg::idn_email_rule));
    BOOST_TEST(mailboxes.has_value());
    const auto located =
        urlg::parse("0/foo#2eb8aa08-aa98-11ea-b4aa-73b441d16380",
                    urlg::tuple_rule(fg::relative_json_pointer_rule,
                                     urlg::squelch(urlg::delim_rule('#')), fg::uuid_rule));
    // A json-pointer token holds "#": the pointer runs to the end.
    BOOST_TEST(!located.has_value());
    const auto template_and_iri =
        urlg::parse("{a}/b http://a/\xC3\xA9",
                    urlg::tuple_rule(fg::uri_template_rule, urlg::squelch(urlg::delim_rule(' ')),
                                     fg::iri_rule));
    BOOST_TEST(template_and_iri.has_value());
    const auto duration_and_reference = urlg::parse(
        "P1D ../x", urlg::tuple_rule(fg::duration_rule, urlg::squelch(urlg::delim_rule(' ')),
                                     fg::uri_reference_rule));
    BOOST_TEST(duration_and_reference.has_value());
    const auto pointer_and_label = urlg::parse(
        "/a/b", urlg::tuple_rule(fg::json_pointer_rule, urlg::optional_rule(fg::idna_label_rule)));
    BOOST_TEST(pointer_and_label.has_value());
    const auto iri_reference = urlg::parse("", fg::iri_reference_rule);
    BOOST_TEST(iri_reference.has_value());
    const auto uri_or_date =
        urlg::parse("2020-01-01", urlg::variant_rule(fg::uri_rule, fg::date_rule));
    BOOST_TEST(uri_or_date.has_value() && uri_or_date->index() == 1);
}

/**
 Labels joined by a grammar's own dots are judged one at a time: the Bidi rule across them is the
 grammar's to check, as idn_hostname_rule does.
*/
void labels_joined_by_a_grammar() {
    const std::string_view name = "0a.\xD7\x90";
    const auto labels = urlg::parse(
        name, urlg::range_rule(urlg::ref(fg::idna_label_rule), urlg::ref(next_label), 1));
    BOOST_TEST(labels.has_value() && labels->size() == 2);
    const auto whole = urlg::parse(name, fg::idn_hostname_rule);
    BOOST_TEST(whole.has_error() && whole.error() == idna::error::bidi);
}

void the_host_and_port_of_the_page() {
    constexpr auto idn_host_port = urlg::tuple_rule(
        fg::idn_hostname_rule,
        urlg::optional_rule(urlg::tuple_rule(urlg::squelch(urlg::delim_rule(':')),
                                             urlg::unsigned_rule<std::uint16_t>{})));
    const auto parsed = urlg::parse(
        "b\xC3\xBC"
        "cher.example:8080",
        idn_host_port);
    if (BOOST_TEST(parsed.has_value())) {
        BOOST_TEST_EQ(std::get<0>(*parsed),
                      "b\xC3\xBC"
                      "cher.example");
        BOOST_TEST(std::get<1>(*parsed).has_value() && *std::get<1>(*parsed) == 8080);
    }
}

void an_idn_rules_error_is_idnas() {
    const auto contexto = urlg::parse(
        "a\xC2\xB7"
        "b",
        fg::idn_hostname_rule);
    BOOST_TEST(contexto.has_error() && contexto.error() == idna::error::contexto);
    const auto hyphen = urlg::parse("-abc", fg::idna_label_rule);
    BOOST_TEST(hyphen.has_error() && hyphen.error() == idna::error::hyphen_at_start_or_end);
    const auto domain = urlg::parse(
        "a@a\xC2\xB7"
        "b",
        fg::idn_email_rule);
    BOOST_TEST(domain.has_error() && domain.error().category() == idna::error_category());
}

}  // namespace

int main() {
    where_each_rule_stops();
    runs_fail_whole();
    rules_inside_the_combinators();
    labels_joined_by_a_grammar();
    the_host_and_port_of_the_page();
    an_idn_rules_error_is_idnas();
    return boost::report_errors();
}
