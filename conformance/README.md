# CSSON conformance suite

**The universal contract.** Every implementation — the `libcsson` core and
every environment binding — must produce the same canonical JSON from the same
document, via a real CSS parser's rule tree (never hand-written parsing; see
`../CLAUDE.md`). The **browser-parity invariant** makes the browsers first-class:
the same output must come from Chrome (Blink) and Firefox (Gecko).

> **The full step-by-step verification runbook is `VERIFICATION.md`** — how each
> engine is driven, prerequisites, how to run, and how the cross-engine result is
> proven (incl. the numeric edge cases).

## Layout — grouped by standard version
```
v1/
  csson_v1-csson.css      the canonical example document
  expected.json       the canonical JSON every reader must emit (byte-identical)
  fixtures/           extra fixtures (numeric-csson.css, edge-csson.css + their .expected.json)
  verify.sh           builds-aware harness: runs each built engine, diffs to expected
  readers/
    browser/ reader.js             the dependency-free reader (browser built-ins ONLY)
             canon_browser.js      drives Chrome (Blink) via chrome-remote-interface/CDP
    firefox/ canon_firefox.js      drives Firefox (Gecko) via geckodriver/WebDriver
```
The **C engine is the `libcsson` core CLI** (`core/build/csson canon`) — there is
no separate C reader in the suite. An earlier `readers/c/` duplicated the core's
read path (and carried the same bugs); it was removed so the suite checks the one
real reference core.
`readers/browser/reader.js` is the single, dependency-free reader (walks the
built-in CSSOM); **both** the Chrome and Firefox drivers inject it verbatim, so
the two browsers run identical reader code.

## The contract
For `v1/csson_v1-csson.css`, every conforming reader emits exactly:
```
MD5 (expected.json) = 9ae94e393bf09a58bb597acee1b5d975   (614 bytes)
```
Object keys sorted, output compact, object arrays in source order (see
`../spec/v1/SPEC.md` §6).

## Running it
```sh
cmake -S core -B core/build -G Ninja && cmake --build core/build   # the C engine = libcsson CLI
cd v1/readers/browser && npm install          # chrome-remote-interface (drives system Chrome via CDP)
# firefox: needs geckodriver + a Firefox binary ($GECKODRIVER / $FIREFOX_BIN; defaults under tmp/tools)
cd v1 && ./verify.sh                           # runs each built engine on every fixture, diffs to expected
```
`verify.sh` skips any engine not built. The C engine is the `libcsson` CLI
(`$CSSON`, default `../../core/build/csson`), invoked as `csson canon`; override
with `CSSON=/path/to/csson ./verify.sh`.

## Verified here
Byte-identical canonical JSON across three engines (two real browsers):
- **C** — the `libcsson` core (lexbor 3.1.0; CSSOM walk, verbatim value via source offsets)
- **Chrome (Blink)** — built-in `cssRules` walk
- **Firefox (Gecko)** — built-in `cssRules` walk

…on `csson_v1-csson.css`, `fixtures/numeric-csson.css`, and `fixtures/edge-csson.css` (the
integer-range / leading-zero / control-character edge cases).

## Scalar value fidelity
Canonical values are the **verbatim source token** (spec §5), so they survive
losslessly and engines cannot disagree: `--pi: 3.14159265358979` and `--sci: 1e3`
read identically (`"3.14159265358979"`, `"1e3"`) in lexbor, Chrome and Firefox.
The C core achieves this by reading lexbor's source offsets (not its
re-serialized CSSOM value); the browsers via `getPropertyValue`, which preserves
the token in both Blink and Gecko.
