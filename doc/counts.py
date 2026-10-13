#!/usr/bin/env python3
# Copyright (c) 2026 WebCpp.org
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt)

"""Counts what lexical's page says of the JSON Schema suite and of V8, which no other count holds.

tools/doc/counts.py runs this script with lexical's directory as its one argument, and gives the
page each `name=<number>` line it prints as an attribute:

* for each dialect, the string tests of the format files the suite runner reads,
  `n-suite-<dialect>`, and of those the ones expected to pass, `n-suite-passed-<dialect>`, the
  first less the dialect's known failures; and their sums, `n-suite` and `n-suite-passed`. The
  unknown format's tests are json_schema's and are not counted;
* `n-regex-patterns`, the patterns test/regex/cases/patterns.json holds, which regex_v8 compares
  with V8, and `n-regex-valid`, those V8 accepts, from test/regex/expected/verdicts.json.

A dialect whose directory holds no format file is a fault, and so is a count of zero, rather than a
zero on the page. A fault is written on the standard error, and the standard output holds only
counts.

Usage: counts.py <library-directory>. Exit 0 printing the counts, 1 naming a fault, 2 on a usage
error.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

DIALECTS = [
    ('draft4', 'draft4/optional/format'),
    ('draft6', 'draft6/optional/format'),
    ('draft7', 'draft7/optional/format'),
    ('draft2019-09', 'draft2019-09/optional/format'),
    ('draft2020-12', 'draft2020-12/optional/format'),
    ('v1', 'v1/format'),
]
UNKNOWN = 'unknown'


class Fault(Exception):
    """A count that cannot be made, said to the user."""


def strings(directory: Path) -> int:
    """The string tests of every format file in `directory`, but the unknown format's."""
    files = sorted(directory.glob('*.json'))
    if not files:
        raise Fault(f'counts.py: no format tests in {directory}')
    count = 0
    for path in files:
        for case in json.loads(path.read_text(encoding='utf-8')):
            if case['schema'].get('format') == UNKNOWN:
                continue
            count += sum(1 for test in case['tests'] if isinstance(test['data'], str))
    return count


def known(library: Path, dialect: str) -> int:
    """The entries of the dialect's known-failure list."""
    path = library / 'test' / 'known_failures' / f'{dialect}.json'
    entries = json.loads(path.read_text(encoding='utf-8'))
    if not isinstance(entries, list):
        raise Fault(f'counts.py: {path} is not a list')
    return len(entries)


def regex_counts(library: Path) -> list[str]:
    """The patterns regex_v8 compares with V8, and those V8 accepts."""
    patterns = json.loads((library / 'test/regex/cases/patterns.json').read_text(encoding='utf-8'))
    verdicts = json.loads(
        (library / 'test/regex/expected/verdicts.json').read_text(encoding='utf-8'))
    if not patterns or len(patterns) != len(verdicts):
        raise Fault('counts.py: test/regex holds no patterns, or not one verdict for each')
    valid = sum(1 for verdict in verdicts if verdict == 1)
    if valid == 0:
        raise Fault('counts.py: V8 accepts none of the patterns of test/regex')
    return [f'n-regex-patterns={len(patterns)}', f'n-regex-valid={valid}']


def main(argv: list[str]) -> int:
    """Prints the counts of the library at argv[1]."""
    if len(argv) != 2:
        print('usage: counts.py <library-directory>', file=sys.stderr)
        return 2
    library = Path(argv[1]).resolve()
    tests = library.parent.parent / 'third_party' / 'libs' / 'JSON-Schema-Test-Suite' / 'tests'
    lines: list[str] = []
    total = 0
    passed = 0
    try:
        for dialect, directory in DIALECTS:
            run = strings(tests / directory)
            expected = run - known(library, dialect)
            lines += [f'n-suite-{dialect}={run}', f'n-suite-passed-{dialect}={expected}']
            total += run
            passed += expected
        lines += [f'n-suite={total}', f'n-suite-passed={passed}']
        lines += regex_counts(library)
    except (Fault, OSError, ValueError, KeyError) as fault:
        print(fault, file=sys.stderr)
        return 1
    print('\n'.join(lines))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
