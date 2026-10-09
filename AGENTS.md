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
