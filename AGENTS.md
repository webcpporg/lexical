# Working on formats

webcpp's rules apply here: read the superproject's
[AGENTS.md](../../AGENTS.md)
(<https://github.com/webcpporg/webcpp/blob/main/AGENTS.md>) first. This file
holds only what is specific to formats.

- **A library of webcpp's own.** formats ports nothing: its evidence is the
  RFCs it cites, its unit tests, and the official JSON-Schema-Test-Suite run
  directly against its validators. It has no oracle and no twins.
- **A format is a rule and a function.** Every format is a Boost.URL rule in
  `webcpp::formats::grammar` and a validator `is_<name>`, which is the rule
  parsing the whole string. A change to one is a change to the other; the
  unit tests check that they agree on every input they hold.
- **Boost.URL is never modified.** formats composes Boost.URL's grammar
  library and adds terminals of its own; where Boost.URL is wrong, formats
  guards it and offers the fix upstream.
- **What raises.** formats raises nothing of its own; only a memory resource
  may throw, and only the IRI functions and `is_regex` take one.
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
- **Why IRIs are mapped.** Boost.URL parses RFC 3986 only. formats checks
  what RFC 3987 adds, the non-ASCII code points and where they may be, then
  maps each of their octets to `%XX` and lets Boost.URL judge the structure:
  a `%XX` is valid exactly where RFC 3987 allows `ucschar`, but in an IP
  literal, where Boost.URL takes it for an RFC 6874 zone identifier, so
  formats refuses a non-ASCII code point there itself. The mapped copy
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
