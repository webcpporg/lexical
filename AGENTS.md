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
