#!/usr/bin/env python3
# Copyright (c) 2026 WebCpp.org
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt)

"""Writes the patterns that lexical's regular expressions are compared with V8 on.

Usage: generate_patterns.py [--cache DIR] [PATTERNS]

The one output is written at the path given, or by default at its place in the library:
test/regex/cases/patterns.json, a JSON array of distinct patterns, sorted, one a line, every
character outside printable ASCII escaped. The oracle of test/oracle asks V8 which of them
`new RegExp(pattern, "u")` accepts and writes its answers in test/regex/expected/; the test
regex_v8 asks lexical's is_regex the same, on every target.

The patterns are of three kinds:

- a grammar's, drawn at random with Python's random.Random from seeds 2 to 9, 3,000 each, as the
  study of Boost.Regex as an ECMA-262 engine drew them (~/.buzz/RESEARCH, "Boost.Regex as
  webcpp's ECMA-262 engine", section 1): atoms, groups of every kind, backreferences, assertions,
  quantifiers and, in 30% of them, one of 41 defects. Each pattern is followed by four subjects
  drawn from the same generator, which this script draws and drops, so that the patterns are the
  study's;
- every name \\p{...} accepts, under each form ECMA-262 gives it, and the same names misspelt, from
  the Unicode files tools/unicode.json pins, read as tools/generate_tables.py reads them;
- every pattern of the JSON-Schema-Test-Suite, the superproject's submodule
  third_party/libs/JSON-Schema-Test-Suite: the value of each `pattern`, each key of a
  `patternProperties`, and each string a test of `"format": "regex"` holds, in every file of its
  tests/;
- the cases the grammar does not draw: ECMAScript 2025's modifiers and duplicate group names,
  lookbehinds of every width, escapes of every kind, classes and quantifiers at their edges.

Two runs write the same bytes. Exit 0 when the output was written, 1 naming a fault, 2 on a usage
error.
"""

from __future__ import annotations

import argparse
import json
import random
import sys
from pathlib import Path

import generate_tables

LIBRARY = Path(__file__).resolve().parent.parent
PATTERNS = 'test/regex/cases/patterns.json'

# The study's grammar, as its fuzz.py wrote it: subjects' characters, atoms, defects and
# quantifiers.
ALPHA = ['a', 'b', '\u00e9', '\U0001F432', '\u03a0', '1', '_', ' ', '\n', '\u2028', '\u00a0', 'K',
         'k', '\u212a', '\u017f', 's', 'S']
ATOMS = ['a', 'b', '\u00e9', '\U0001F432', '\\d', '\\D', '\\w', '\\W', '\\s', '\\S', '.', '\\p{L}',
         '\\P{L}', '\\p{Lu}', '\\p{Script=Greek}', '\\p{scx=Grek}', '\\p{Nd}', '\\p{ASCII}',
         '[a-c]', '[^a]', '[\\d\\s]', '[\\D\\S]', '[^\\W\\d]', '[\\p{L}\\d]', '[^\\p{L}a]', '[]',
         '[^]', '\\u{1F432}', '\\uD83D\\uDC32', '\\x61', '\\u00e9', '\\n', '[\\n]', '\\cJ', '\\0',
         '[\\b]', '\\/', '\\.', '[-a]', '[a-]', '[\\-]', 'k', 'K', 's', '\\u212a', '[k-m]', '[A-Z]',
         '[^\\s]', '[\\S\\s]', '[\\w-]']
BAD = ['{', '}', ']', '\\a', '\\-', '(?', '\\c', '\\x1', '\\u{110000}', '\\k', '\\8', '[z-a]',
       '[\\d-z]', '**', '(?i)', '\\p{Foo}', '\\p{letter}', '\\pL', '(?<a', '(?<1a>x)', '\\Z', '\\A',
       'a{2,1}', '(?P<n>x)', '(?#c)', '\\q', '\\01', '[\\1]', '(?=a)*', '(?<=a)?', '\\u12', 'a{,5}',
       '(?ii:a)', '(?-:a)', '(?i-i:a)', '\\p{sc=Foo}', '\\p{General_Category=Greek}',
       '\\p{Script=L}', '(?<n>a)(?<n>b)', ')', '[', '(']
QUANT = ['*', '+', '?', '{2}', '{1,}', '{0,2}', '*?', '+?', '??', '{1,2}?']
GROUPS = ['(', '(?:', '(?<n%d>', '(?=', '(?!', '(?<=', '(?<!', '(?i:', '(?m:', '(?s:', '(?-i:']
ASSERTIONS = ('^', '$', '\\b', '\\B')
SEEDS = range(2, 10)
PATTERNS_PER_SEED = 3000
SUBJECTS_PER_PATTERN = 4

# The cases the grammar does not draw, each chosen for an edge of ECMA-262's u mode.
EDGES = [
    # ECMAScript 2025's modifiers.
    '(?i:a)', '(?-i:a)', '(?i-:a)', '(?ims-:a)', '(?i-m:a)', '(?ims:a)', '(?-ims:a)', '(?-:a)',
    '(?ii:a)', '(?i-i:a)', '(?i-s-m:a)', '(?x:a)', '(?i)', '(?i', '(?i:', '(?i:a', '(?:a', '(?',
    # ECMAScript 2025's duplicate group names.
    '(?<n>a)|(?<n>b)', '(?:(?<n>a)|(?<n>b))\\k<n>', '(?:(?<n>a)|(?<n>b))|(?<n>c)',
    '(?<n>a)(?<n>b)', '((?<n>a)|b)(?<n>c)', '(?<n>a)|(?:(?<n>b)|c)(?<n>d)', '(?<n>(?<n>a))',
    '(?<=(?<n>a)|(?<n>b))c', '(?<n>a)|(?<=(?<n>b))', '(?:(?<n>a)|b)*(?<n>c)',
    '((?<n>a)|(?<n>b))*', '(?<n>a)|(?<m>b)|(?<n>c)\\k<m>', '\\k<n>(?<n>a)|(?<n>b)',
    # Lookbehinds of every width, and backreferences in and out of them.
    '(?<=\\$\\d*)x', '(?<=a+)b', '(?<=a|bc)d', '(?<=(?:ab|c))d', '(?<!x{1,3})y', '(?<=(a))\\1',
    '(?<=\\1(a))b', '(?<=a*)d', '(?<=a{2,})b', '(?<=(?<=a+)b)c', '(?<!(?:a|bb)c|d)e',
    '\\1(a)', '(a\\1)', '\\2(a)', '(a)\\10', '\\k<n>(?<n>a)', '\\k<m>(?<n>a)', '\\k<n', '\\k',
    '(?<a\\u0062>x)\\k<ab>', '(?<\\u{61}>x)', '(?<$_\u00e9>x)', '(?<\u00e9>x)', '(?<1a>x)',
    '(?<>x)',
    '(?<a b>x)', '(?<a\\x62>x)',
    # Escapes.
    '\\u{1F432}', '\\u{0}', '\\u{10FFFF}', '\\u{110000}', '\\u{}', '\\u{g}', '\\uD83D\\uDC32',
    '\\uD83D', '\\uDC32', '\\uD83Dx', '\\x61', '\\x6', '\\cJ', '\\cj', '\\c1', '\\c_', '\\0',
    '\\00', '\\01', '\\8', '\\9', '\\/', '\\-', '\\a', '\\e', '\\A', '\\Z', '\\z', '\\G', '\\Q',
    '\\E', '\\h', '\\R', '\\X', '\\N', '\\K', '\\_', '\\ ', '\\\\A', '\\\\\\A', '\\p', '\\P',
    '\\p{', '\\p{}', '\\p{L', '\\pL', '\\p{L}+', '\\P{L}*',
    # Classes.
    '[]', '[^]', '[\\b]', '[\\B]', '[\\-]', '[a-]', '[-a]', '[a-b-c]', '[\\d-z]', '[z-\\d]',
    '[z-a]', '[a-a]', '[\\c_]', '[\\k]', '[\\1]', '[\\0]', '[\\00]', '[\\D\\S]', '[\\W\\d]',
    '[^\\W\\d]', '[\\P{L}\\P{N}]', '[^\\P{L}\\P{N}]', '[[:alpha:]]', '[[a]]', '[a', '[\\]',
    '[\\u{1F432}-\\u{1F433}]', '[\\uD83D\\uDC32-\\uD83D\\uDC33]',
    '(?i:[\\W\\d])', '(?i:[^\\P{Lu}])', '(?i:\\b)', '(?i:\\B)',
    # Quantifiers and assertions.
    'a{2}', 'a{2,}', 'a{2,3}', 'a{,3}', 'a{3,2}', 'a{2', 'a{', 'a}', 'a{}', 'a{a}', '{', '}', ']',
    'a**', 'a*?', 'a*??', 'a+*', 'a{2}*', '^*', '$+', '\\b?', '\\B{2}', '(?=a)*', '(?!a)+',
    '(?<=a)?', '(?<!a){1}', 'a|', '|', '||', '(|)', '()', '(?:)', '^$', '.*',
    'a{4294967295}', 'a{4294967296}', 'a{99999999999999999999}',
    # Empty groups and lookarounds, and alternatives that are empty.
    '(?=)', '(?!)', '(?<=)', '(?<!)', '(?=|)', '(?<=|a)', '(?<!a|)', 'a(?=)b', '(?=(?=))',
    '(?<=(?<=))', '(?:|a)', '(|)\\1', '(?i:)', '(?<n>)\\k<n>',
    # ignoreCase, captures in repetitions, raw line terminators, NUL and the sign of a delimiter.
    '(?i:\\p{Lu})', '(?i:[^\\p{Lu}])', '(?i:\\W)', '(?i:[\\w\\W])', '(a)*\\1',
    '^(?:(a)|b)+\\1$', '(?:(a)|(b))+\\1\\2', 'a\nb', 'a\u2028b', '[\n\u2029]', 'a\x00b',
    '/', '[/]', '\\/', '[\\p{L}-z]', '[a-\\p{L}]', '[\\w-\\d]',
    # Other dialects' groups.
    '(?P<n>x)', '(?#c)', '(?>a)', '(?|a)', '(?R)', '(?1)', '(?(1)a)', '(?&n)', '(*ACCEPT)',
]


def fuzz_pattern(rng: random.Random, depth: int = 0, groups: list[str] | None = None) -> str:
    """One pattern of the study's grammar, drawn from `rng` as the study's fuzz.py drew it."""
    if groups is None:
        groups = []
    parts: list[str] = []
    for _ in range(rng.randint(1, 3)):
        r = rng.random()
        if r < 0.45 or depth > 1:
            atom = rng.choice(ATOMS)
        elif r < 0.6:
            inner = fuzz_pattern(rng, depth + 1, groups)
            kind = rng.choice(GROUPS)
            if kind == '(?<n%d>':
                kind = kind % len(groups)
            if kind == '(' or kind.startswith('(?<n'):
                groups.append(kind)
            atom = kind + inner + ')'
        elif r < 0.7 and groups:
            k = rng.randrange(len(groups))
            named = '\\k<n%d>' % k if groups[k].startswith('(?<n') else '\\%d' % (k + 1)
            atom = rng.choice(['\\%d' % (k + 1), named])
        elif r < 0.8:
            atom = rng.choice(ASSERTIONS)
        else:
            atom = '(?:' + fuzz_pattern(rng, depth + 1, groups) + '|' + \
                   fuzz_pattern(rng, depth + 1, groups) + ')'
        lookaround = atom.startswith(('(?=', '(?!', '(?<=', '(?<!'))
        if rng.random() < 0.35 and atom not in ASSERTIONS and not lookaround:
            atom += rng.choice(QUANT)
        parts.append(atom)
    pattern = ''.join(parts)
    if depth == 0 and rng.random() < 0.3:
        position = rng.randrange(len(pattern) + 1)
        pattern = pattern[:position] + rng.choice(BAD) + pattern[position:]
    return pattern


def fuzz_patterns() -> set[str]:
    """The study's patterns: seeds 2 to 9, 3,000 each, their subjects drawn and dropped."""
    found: set[str] = set()
    for seed in SEEDS:
        rng = random.Random(seed)
        for _ in range(PATTERNS_PER_SEED):
            found.add(fuzz_pattern(rng))
            for _ in range(SUBJECTS_PER_PATTERN):
                for _ in range(rng.randint(0, 7)):
                    rng.choice(ALPHA)
    return found


def property_patterns(unicode: generate_tables.Unicode) -> set[str]:
    """Every name \\p{...} accepts, under each of its forms, and each misspelt."""
    categories = sorted({name for names in unicode.categories for name in names})
    scripts = sorted({name for names in unicode.script_names for name in names})
    binary = sorted(unicode.binary_aliases)
    found: set[str] = set()
    for name in categories:
        found |= {f'\\p{{{name}}}', f'\\P{{{name}}}', f'\\p{{gc={name}}}',
                  f'\\p{{General_Category={name}}}', f'[\\p{{{name}}}]', f'\\p{{sc={name}}}',
                  f'\\p{{{name.lower()}}}', f'\\p{{{name.upper()}}}'}
    for name in binary:
        found |= {f'\\p{{{name}}}', f'\\P{{{name}}}', f'\\p{{{name.lower()}}}',
                  f'\\p{{gc={name}}}', f'\\p{{{name}=Yes}}'}
    for name in scripts:
        found |= {f'\\p{{sc={name}}}', f'\\p{{Script={name}}}', f'\\p{{scx={name}}}',
                  f'\\p{{Script_Extensions={name}}}', f'\\P{{scx={name}}}', f'\\p{{{name}}}',
                  f'\\p{{sc={name.lower()}}}', f'\\p{{gc={name}}}'}
    for form in ('gc', 'General_Category', 'sc', 'Script', 'scx', 'Script_Extensions',
                 'GC', 'general_category', 'Sc', 'script', 'Block', 'blk', 'Bidi_Class'):
        found |= {f'\\p{{{form}}}', f'\\p{{{form}=}}', f'\\p{{{form}=L}}', f'\\p{{{form}=Greek}}'}
    return found


def suite_patterns(suite: Path) -> set[str]:
    """Every pattern the suite's schemas and its tests of the format regex hold."""
    if not (suite / 'draft2020-12').is_dir():
        raise generate_tables.Fault(f'{suite} holds no draft2020-12: check out the submodule with '
                                    '`git submodule update --init '
                                    'third_party/libs/JSON-Schema-Test-Suite`')
    found: set[str] = set()

    def walk(node: object) -> None:
        if isinstance(node, dict):
            for key, value in node.items():
                if key == 'pattern' and isinstance(value, str):
                    found.add(value)
                elif key == 'patternProperties' and isinstance(value, dict):
                    found.update(value)
                walk(value)
        elif isinstance(node, list):
            for item in node:
                walk(item)

    for path in sorted(suite.rglob('*.json')):
        for case in json.loads(path.read_text(encoding='utf-8')):
            walk(case['schema'])
            if isinstance(case['schema'], dict) and case['schema'].get('format') == 'regex':
                found.update(test['data'] for test in case['tests']
                             if isinstance(test['data'], str))
    return found


def generate(cache: Path | None, outputs: list[Path]) -> None:
    """Draws every pattern and writes them, or nothing."""
    _, inputs = generate_tables.read_inputs(cache)
    suite = generate_tables.SUPERPROJECT / 'third_party/libs/JSON-Schema-Test-Suite/tests'
    patterns = sorted(fuzz_patterns() | property_patterns(generate_tables.Unicode(inputs))
                      | suite_patterns(suite) | set(EDGES))
    lines = ',\n'.join('    ' + json.dumps(pattern, ensure_ascii=True) for pattern in patterns)
    path = outputs[0] if outputs else LIBRARY / PATTERNS
    generate_tables.write(path, f'[\n{lines}\n]\n'.encode('ascii'))


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description='Writes the patterns lexical\'s regular expressions are compared with V8 on.')
    parser.add_argument('--cache', type=Path, default=None,
                        help='where the pinned files are kept (default: bin/unicode/<version>)')
    parser.add_argument('outputs', type=Path, nargs='*',
                        help=f'where {PATTERNS} is written (default: its place in the library)')
    arguments = parser.parse_args(argv)
    if len(arguments.outputs) > 1:
        parser.error(f'give no output path, or one for {PATTERNS}')
    try:
        generate(arguments.cache, arguments.outputs)
    except (generate_tables.Fault, OSError, KeyError, ValueError) as fault:
        print(f'generate_patterns.py: {fault}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
