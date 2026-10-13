// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Asks V8 which patterns of the cases directory are ECMA-262 regular expressions in u mode, the
// verdicts lexical's is_regex must give: `new RegExp(pattern, "u")` throws a SyntaxError on the
// others. It writes, in the output directory, verdicts.json, one verdict a line, 1 or 0, in the
// order of patterns.json.
//
// Usage: node v8.mjs <cases directory> <output directory>

import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

const [cases, output] = process.argv.slice(2);
if (cases === undefined || output === undefined) {
  console.error('usage: node v8.mjs <cases directory> <output directory>');
  process.exit(2);
}

const patterns = JSON.parse(readFileSync(join(cases, 'patterns.json'), 'utf8'));
const verdicts = patterns.map((pattern) => {
  try {
    new RegExp(pattern, 'u');
    return '    1';
  } catch (error) {
    if (!(error instanceof SyntaxError)) {
      throw error;
    }
    return '    0';
  }
});
mkdirSync(output, { recursive: true });
writeFileSync(join(output, 'verdicts.json'), `[\n${verdicts.join(',\n')}\n]\n`);
