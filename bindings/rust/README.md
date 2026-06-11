# CSSON for Rust

Status: **planned** (scaffold). Thin wrapper over the C core (`libcsson`, on
liblexbor) in `../../core`.

| | |
|---|---|
| **Environment** | Rust |
| **Front door** | C ABI (`libcsson` / `csson.h`) |
| **Mechanism** | `csson-sys` (raw FFI via bindgen) + a safe `csson` crate wrapping it |
| **Distribution** | crates.io: `csson` (safe API) and `csson-sys` (raw bindings) |

## A binding, not a reimplementation
Rust links **`libcsson`** (which is lexbor) over the C ABI. It does **not** parse
CSS itself: an earlier standalone Rust implementation was removed precisely
because it duplicated parsing/processing, which the prime directive forbids (see
`../../CLAUDE.md`). The crate is a safe veneer over the one reference core, so its
output is the same bytes as every other engine.

## Contract
CSSON never writes its own parsing (see `../../CLAUDE.md`): this binding reads via
a real CSS parser (lexbor, through `libcsson`) and only walks the rule tree. It
MUST emit **byte-identical canonical JSON** for the shared suite in
`../../conformance`.

## Build outline
1. Build the C ABI in `../../core` (`libcsson` + `csson.h`).
2. `csson-sys`: generate FFI bindings (bindgen over `csson.h`); link `libcsson`,
   `lexbor` and `yyjson` from a `build.rs` (the `cc`/`cmake` crate), or link a
   prebuilt static lib.
3. `csson`: a safe API — `to_canonical_json(&str) -> Result<String>`, `set`,
   `remove`, `patch` — that owns the FFI strings and frees them via
   `csson_free_string`. Errors map the `char **err` out-param to `Result`.
4. Run `../../conformance` fixtures from a Rust test; output must match
   `expected.json`.

## Versions & platforms
- **Supported CSSON standard versions:** 1 (forwarded to the core; never branched on).
- **Architectures:** amd64 + ARM64, distributed per `../../core/PLATFORMS.md`.
