# The CSSON standard

This directory is the **single, canonical definition of what CSSON is**. Every
implementation in this repository implements the spec here; the `../conformance`
fixtures prove it. If code and spec disagree, **the spec wins**.

## Current version
- **CSSON v1** — `v1/SPEC.md`. This is the current — and only — standard.
- All implementations read from a real CSS parser's rule tree (lexbor); CSSON
  never writes its own parsing — see `../CLAUDE.md`.

## Two independent version numbers
1. **Standard version** — a single integer (CSSON v1, v2, …). A new integer means
   a breaking change to the grammar/semantics. Each version is frozen behind a
   version selector; once ratified it never changes.
2. **Implementation version** — each library's own semver. One build can support
   several standard versions and advertises which (`csson_supported_versions`).

The version is selected by the caller (ABI `version` arg / `--version N`,
0 = current). CSSON v1 has **no in-file version marker** — that would be data the
format invents on top of CSS, which the directive forbids.

## Layout
```
spec/
  README.md     # this file
  v1/SPEC.md    # normative definition of CSSON v1 (current)
  v2/SPEC.md    # (future) added beside v1; v1 is never edited
```
Each `vN/SPEC.md` is mirrored by a frozen version path in the C core
(`../core`, on liblexbor) and a fixture suite (`../conformance/vN`).

## Adding a future version
1. Draft & ratify `spec/vN/SPEC.md`; **freeze** it.
2. Add a frozen `vN` path to the core (never edit `v1`).
3. Extend the version selector and add `vN` fixtures to conformance.
4. Bump each library's semver and extend its advertised supported versions.
