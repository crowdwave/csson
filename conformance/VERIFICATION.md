# Cross-engine verification process

How we prove the **browser-parity invariant** (`../CLAUDE.md`): a CSSON document
must read to the **same canonical JSON** whether processed by the `csson` core or
by a real browser's built-in CSS engine. This runbook is reproducible end to end.

## What is being verified

One document → one canonical JSON, byte-identical across three independent engines
(two of them real browsers):

| Engine | What it is | How it's driven | Reads the value via |
|---|---|---|---|
| **csson core (C)** | `libcsson` on lexbor (C) — the reference reader | `core/build/csson canon <file>` | lexbor source offsets (verbatim) |
| **Chrome** | Blink, a real browser | `readers/browser/canon_browser.js` → CDP (chrome-remote-interface) | built-in CSSOM `getPropertyValue` |
| **Firefox** | Gecko, a real browser | `readers/firefox/canon_firefox.js` → WebDriver (geckodriver) | built-in CSSOM `getPropertyValue` |

The C engine is the `libcsson` core CLI itself; the suite no longer carries a
separate `readers/c` (it duplicated the core read path and its bugs, so it was
removed).

**Single reader, two browsers.** Both browser drivers inject the exact same
dependency-free reader, `readers/browser/reader.js` (browser built-ins only). So
Chrome and Firefox are tested with identical reader code; any difference is a
genuine engine difference, not a code difference. `packages/typescript` ships the
same reader for end users.

## Prerequisites (per engine; each is optional — `verify.sh` skips what's absent)

```sh
# csson core = the C engine (lexbor; clones + builds a trimmed lexbor)
cmake -S ../core -B ../core/build -G Ninja && cmake --build ../core/build   # -> core/build/csson

# Chrome (Blink): a system Chrome + the CDP client
( cd v1/readers/browser && npm install )     # chrome-remote-interface (driver only)
#   Chrome binary via $CHROME_PATH or `google-chrome` on PATH.

# Firefox (Gecko): stock Mozilla Firefox + geckodriver (no npm deps; uses Node fetch)
( cd v1/readers/firefox && ./setup.sh )      # fetches both into tmp/tools/
#   (or set $FIREFOX_BIN / $GECKODRIVER to an existing Firefox + geckodriver)
```

The browser drivers launch their browser **headless** and never touch
`getComputedStyle` — they walk the authored `cssRules` tree only.

## Run it

```sh
cd v1 && ./verify.sh
```

`verify.sh` runs every built engine on every fixture and diffs each result to the
committed expected JSON. Expected output:

```
== csson_v1-csson.css ==
  c: identical
  browser: identical
  firefox: identical
== numeric-csson.css ==
  c: identical
  browser: identical
  firefox: identical
== edge-csson.css ==
  c: identical
  browser: identical
  firefox: identical

RESULT: all checked readers match expected
```

Run a single engine by hand (each prints canonical JSON to stdout):

```sh
../core/build/csson canon v1/csson_v1-csson.css                          # C core
node v1/readers/browser/canon_browser.js  "$PWD/v1/csson_v1-csson.css"   # Chrome
node v1/readers/firefox/canon_firefox.js  "$PWD/v1/csson_v1-csson.css"   # Firefox
```

## Fixtures

- `v1/csson_v1-csson.css` → `v1/expected.json` — the canonical document
  (MD5 `9ae94e393bf09a58bb597acee1b5d975`, 614 bytes).
- `v1/fixtures/numeric-csson.css` → `v1/fixtures/numeric.expected.json` — scalar edge
  cases: floats, exponent (`1e3`), negatives, units (`px`/`s`), percentage,
  identifier, quoted string, nesting. This fixture is what catches engine
  serialization differences.
- `v1/fixtures/edge-csson.css` → `v1/fixtures/edge.expected.json` — integer-coercion
  boundaries: the JS-safe range (`±(2^53−1)` stays a number, `2^53` and a 20-digit
  value become strings), non-canonical numerics (`007`, `-0`, `1e3`, `3.14`), and a
  control character (`\t`) exercising JSON string escaping. Proves the C core and
  both browsers agree on the exact integer rule (spec §5).

**Expected files are generated from the core** and committed:
```sh
../core/build/csson canon v1/csson_v1-csson.css          > v1/expected.json
../core/build/csson canon v1/fixtures/numeric-csson.css  > v1/fixtures/numeric.expected.json
../core/build/csson canon v1/fixtures/edge-csson.css     > v1/fixtures/edge.expected.json
```
A change to expected output must be a deliberate regeneration, reviewed as a diff.

## Why this is trustworthy (scalar fidelity)

Canonical values are the **verbatim source token** (spec §5): the core reads them
from lexbor's source offsets, the browsers from CSSOM `getPropertyValue`. Both
Blink and Gecko preserve the token, so numeric edge cases agree exactly —
verified live:

```
--sci: 1e3;               -> "sci":"1e3"                   (not 1000)
--pi: 3.14159265358979;   -> "pi":"3.14159265358979"       (full precision)
```

This is the whole point of reading verbatim: there is nothing left for the
engines to normalize differently.

## Extending the suite

- **New fixture:** add `v1/fixtures/<name>-csson.css`, generate
  `v1/fixtures/<name>.expected.json` from the core, then add a `run_doc` line in
  `verify.sh` listing the engines it applies to.
- **New engine/binding:** drive it to emit canonical JSON for the fixtures and
  diff to the expected files. Per the prime directive it must parse with a real
  CSS parser; per the browser-parity invariant its output must match these
  bytes exactly.
