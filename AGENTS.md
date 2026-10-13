# Working on formats

webcpp's rules apply here: read the superproject's
[AGENTS.md](../../AGENTS.md)
(<https://github.com/webcpporg/webcpp/blob/main/AGENTS.md>) first. This file
holds only what is specific to formats.

- **A library of webcpp's own.** formats ports nothing: its evidence is the
  RFCs it cites, its unit tests, the official JSON-Schema-Test-Suite run
  directly against its validators, and V8, whose verdicts on patterns
  `regex_v8` compares `is_regex` with. Its oracle, `test/oracle`, is V8 alone
  (no package), and it has no twins.
- **A format is a rule and a function.** Every format is a Boost.URL rule in
  `webcpp::formats::grammar` and a validator `is_<name>`, which is the rule
  parsing the whole string. A change to one is a change to the other; the
  unit tests check that they agree on every input they hold.
- **Boost.URL is never modified.** formats composes Boost.URL's grammar
  library and adds terminals of its own; where Boost.URL is wrong, formats
  guards it and offers the fix upstream.
- **What raises.** formats raises nothing of its own; only an allocation may
  throw: a memory resource, which only the IRI functions and `is_regex` take,
  and the global `operator new`, through which Boost.Regex compiles.
- **The JSON Schema suite.** `test/suite_test.cpp` runs the format tests of
  the JSON-Schema-Test-Suite, the superproject's submodule
  `third_party/libs/JSON-Schema-Test-Suite`, against the validators. No
  copy of the suite's data is committed: it is MIT data, and every file of
  webcpp is BSL-1.0. `test/known_failures/<dialect>.json` lists the tests
  expected to fail, each with its reason, and only a residue tied to a
  Unicode version may be one; the runner fails on an unlisted failure and on
  a listed test that passes or that the suite does not hold, so a list never
  lies. Moving the submodule is a change of its own, with every new failure
  fixed or listed in it.
- **The uri-reference guard.** `detail/uri_guard.hpp` refuses what Boost.URL
  1.92's `uri_reference_rule` wrongly accepts: a relative reference whose
  path after an authority is not empty and not led by `/`
  (`refuses_relative_authority_path`). With a scheme, Boost.URL stops the URI
  at that authority instead, and `refuses_authority_before_segment` makes the
  rule refuse it too, so that a grammar meets `error::invalid` either way.
  The guard stays until Boost.URL refuses those references itself:
  `uri_test`'s `boost_url_still_needs_the_guard` fails the day the configured
  Boost does, and `refuses_relative_authority_path` then leaves in a change
  of its own, with a changelog line, while `refuses_authority_before_segment`,
  `the_guard` and `the_rule_inside_a_grammar` stay.
- **The bracket guard.** Boost.URL 1.92's query characters hold `[` and `]`
  (`boost/url/rfc/detail/charsets.hpp`, `query_chars`), which RFC 3986
  holds only around an IP literal. `refuses_bracket_in_query` refuses them
  in formats' `uri_rule` and `uri_reference_rule`, so `is_uri`,
  `is_uri_reference` and the IRIs, which parse their mapping with those
  rules, refuse them alike. `uri_test`'s
  `boost_url_still_takes_brackets_in_a_query` fails the day the configured
  Boost refuses them itself, and the guard then leaves in a change of its
  own, with a changelog line, while `brackets_only_around_an_ip_literal`
  stays.
- **The fragment guard.** Boost.URL 1.92's fragment characters hold `#`
  (`boost/url/rfc/detail/charsets.hpp`, `fragment_chars`, on purpose
  upstream), which RFC 3986 3.5 refuses in a fragment.
  `refuses_number_sign_in_fragment` refuses it in formats' `uri_rule` and
  `uri_reference_rule`, wired beside the bracket guard, so every form
  agrees; a `%23` stays valid. `uri_test`'s
  `boost_url_still_takes_a_number_sign_in_a_fragment` fails the day the
  configured Boost refuses it itself, and the guard then leaves in a change
  of its own, with a changelog line, while
  `one_number_sign_before_the_fragment` stays.
- **The zone guard.** Boost.URL 1.92's IP literal takes RFC 6874's zone
  identifier (`boost/url/rfc/detail/impl/ipv6_addrz_rule.hpp`), which RFC
  9844 withdrew from RFC 3986's syntax in 2025 when it obsoleted RFC 6874.
  `refuses_zone_identifier` refuses it in formats' `uri_rule` and
  `uri_reference_rule`, wired beside the fragment guard. `uri_test`'s
  `boost_url_still_takes_a_zone_identifier` fails the day the configured
  Boost refuses one itself, and the guard then leaves in a change of its
  own, with a changelog line, while `no_zone_identifier` stays.
- **What formats keeps of Boost.URL.** Compared with RFC 3986's ABNF, one
  verdict of Boost.URL 1.92 is stricter than RFC 3986, which formats keeps,
  as it never loosens Boost.URL: an `IPvFuture` led by an uppercase `V`
  (RFC 5234 makes the ABNF's `"v"` case-insensitive) is refused.
  `uri_test`'s `ipvfuture_keeps_boost_urls_verdict` fails the day Boost
  takes it: its two verdicts and `iri_test`'s on `http://[V1.x]/` then flip
  to valid, RFC 3986's, and this bullet and the page's "Known deviations"
  lose it.
- **Why IRIs are mapped.** Boost.URL parses RFC 3986 only. formats checks
  what RFC 3987 adds, the non-ASCII code points and where they may be, then
  maps each of their octets to `%XX` and lets Boost.URL judge the structure:
  a `%XX` is valid exactly where RFC 3987 allows `ucschar`. formats still
  refuses a non-ASCII code point in an IP literal before mapping, which keeps
  that verdict independent of the zone guard. The mapped copy
  is the one allocation of formats' validators but `is_regex`'s, and only
  beyond 1,024 mapped octets; an ASCII IRI is never copied.
- **Boost.URL's compiled part.** `/webcpp//boost_url`, which `build.jam`
  names, builds Boost.URL from Boost's sources for every target: `ipv4_address`
  is defined there. On MSVC, which never defines
  `BOOST_URL_HAS_CXX20_CONSTEXPR`, even the grammar's error category is
  defined there, so no format links without it.
- **idna.** formats asks `webcpp::idna`'s public interface, never its
  `detail` (the include boundary checks it): a host name's A-label, in any
  case, is lowercased and judged by `idna::check_label`, one label at a
  time. A verdict of idna's that a test of formats finds wrong becomes a
  test in idna, red first, reported to idna's track; formats never works
  around it.
- **Internationalized names.** `idn_hostname_rule` and `idna_label_rule`
  hand their whole run to `idna::to_ascii` with its default options, into a
  stack buffer of `idna::max_domain_length` octets. A label's run stops at
  `.`, U+3002, U+FF0E and U+FF61, the only code points UTS 46 maps to a
  sequence holding a dot in idna's tables (Unicode 17.0.0), which
  `hostname_test` checks by converting every code point between two letters,
  so that a change of idna's Unicode version that adds one fails it. An
  internationalized Mailbox's lengths, 64 octets for its local part and 254
  for the whole, are measured as written, in UTF-8.
- **Boost.Regex.** `<webcpp/formats/regex.hpp>` is the only header that
  includes Boost.Regex and the regex tables, which `meta/include-boundaries.json`
  holds, and `<webcpp/formats.hpp>` leaves it out. Boost.Regex is a dependency
  of formats, header-only, approved by the owner on 2026-10-12 with the
  design: `is_regex` reads a pattern with formats' translator of ECMA-262
  (`detail/regex_translator.hpp`) and compiles its translation with
  Boost.Regex over a traits class of formats' own (`detail/regex_traits.hpp`),
  as the study of Boost.Regex as an ECMA-262 engine measured it. Its cost,
  which the page and the header's Doc Comment state: Boost.Regex's
  `basic_regex` has no allocator, so its compilation allocates through the
  global `operator new`, 5 to 11 times for a short pattern, and this one header
  does not keep formats' allocation rule. Boost.Regex refuses a lookbehind whose
  width varies, which ECMA-262 allows; `is_regex` then compiles the
  translation with its lookbehinds written as lookaheads, and any other refusal
  of Boost.Regex answers `false`. Never change the translation without running
  `regex_v8` on every target.
- **The regex tables.** `detail/regex_tables.hpp` is generated by
  `tools/generate_tables.py` from the Unicode files `tools/unicode.json` pins
  (Unicode 17.0.0, as idna's), and `test/regex/cases/patterns.json` by
  `tools/generate_patterns.py`, which draws the study's patterns from Python's
  `random` with its seeds, reads every property name from the same files and
  every pattern of the JSON-Schema-Test-Suite. Neither is edited by hand: the
  own lane `generated` checks them, and `update-generated` writes them. V8's
  verdicts, `test/regex/expected/verdicts.json`, are written by the oracle's
  `update-expected` alone, with the CI's Node, 26.7.0; a Node whose V8 or
  Unicode differs may write other verdicts, which is a change of its own.
- **The bounds of `is_regex`.** `max_regex_length`, `max_regex_nesting` and
  `max_regex_alternatives` keep a hostile pattern within a WebAssembly
  program's stack and memory; the deepest pattern at the bounds needs about
  20 KiB of wasm32-wasip2's 64 KiB stack at `-O0` (measured 2026-10-12 with
  wasi-sdk 34). A change of a bound, of the translator's recursion or of
  Boost is measured again, and `regex_test`'s hostile patterns run on every
  target.
