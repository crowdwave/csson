# CSSON

**Structured data that is also valid CSS.** A node is a CSS style rule, fields are
custom properties (`--key`), nested objects are nested rules, and repeated sibling
rules become arrays. The point is to carry structured data through pipelines that
already parse CSS — without adding a second format or parser.

This repository's goal: provide everything needed to use CSSON across the
**major programming environments**, from a single standard that every
implementation reads the same way.

## Prime directive
> **CSSON never writes its own parsing/processing.** Every implementation reads
> from a real CSS parser's authored rule tree and only maps that tree to data. No
> hand-written tokenizers, scanners, brace-walkers, or regex extraction. The
> system is based on **liblexbor**; the reference core is C over lexbor. See
> `CLAUDE.md`.

## Layout
```
csson/
  CLAUDE.md         project directives (prime directive above)
  spec/             the standard — the single canonical definition (versioned)
    v1/SPEC.md      normative CSSON v1 (current)
  core/             the C reference core on liblexbor: libcsson (read + edit) + the csson CLI
                    include/csson.h (C ABI), src/, cli/, build.sh, PLATFORMS.md
  packages/
    typescript/     the JS/TS environment (wraps a real CSS parser)
  bindings/         one wrapper per environment, each links libcsson (or lexbor-via-WASM)
    wasm/ nodejs/ rust/ python/ go/ java/ dotnet/ c/ ruby/ php/ shell/
  conformance/      canonical document + expected JSON + independent multi-engine verifier
    v1/readers/     c (lexbor) · chrome (Blink) · firefox (Gecko)
    VERIFICATION.md the cross-engine (core / Chrome / Firefox) verification runbook
  docs/             css-data-complete-guide.html — the complete guide (live demo, open in a browser)
```

## Versioning
- **Standard version** — a single integer (CSSON v1, v2, …); the caller selects it
  (default = current). **v1 is the current — and only — standard.** A future v2
  (if needed) is added beside v1, which is then frozen.
- **Implementation version** — each library's own semver, independent of the
  standard version.

## Architectures
amd64 (x86-64) and ARM64 are first-class across Linux/macOS/Windows, plus wasm.
CPU architecture never affects semantics. See `core/PLATFORMS.md`.

## Status
- `spec/v1` — current standard (draft).
- `conformance/v1` — canonical doc + `expected.json` (614 B, MD5 `9ae94e…`) plus
  a numeric edge-case fixture. Byte-identical across **C (lexbor)**, **Chrome
  (Blink)**, **Firefox (Gecko)**, and the core — incl. floats/exponents. The
  browser reader uses **only built-in CSSOM** (no deps); `packages/typescript`
  ships it for end users.
- `core` — **working**: `libcsson` + the `csson` CLI on lexbor. Reads canonical
  JSON (matches `expected.json`) and performs **comment-preserving** edits
  (`set`/`rm`) and **RFC 6902 JSON Patch** (`patch`, atomic) via byte-splicing at
  lexbor offsets. Patch JSON parsed by vendored yyjson (MIT, stack-safe).
- `packages/typescript`, `bindings/*` — scaffolds; each links `libcsson` (or
  lexbor compiled to WASM) for its environment.

See `docs/css-data-complete-guide.html` — the complete CSSON v1 guide; open it in
any browser, the demo reads CSSON live via the built-in CSSOM (no server, no deps).
