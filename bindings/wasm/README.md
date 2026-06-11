# CSSON for WebAssembly

Status: **planned** (scaffold). The portable front door: liblexbor + the C core
(`libcsson`, in `../../core`) compiled to WebAssembly.

| | |
|---|---|
| **Environment** | WebAssembly (browsers, Node.js, Deno, any wasm runtime) |
| **Front door** | lexbor + libcsson → `.wasm` |
| **Mechanism** | Emscripten build of `../../core` exposing the C ABI (`csson.h`) via `cwrap`/`ccall` |
| **Distribution** | npm `@csson/wasm` (ESM + CJS), plus the raw `.wasm` for any host |

## Why WASM exists
Two jobs no other front door can do everywhere:
- **In-browser editing.** The native CSSOM exposes no source byte offsets and
  editing via `cssText` strips comments, so `set`/`remove`/`patch` run lexbor
  compiled to WASM — the *same* engine as the native core, so the result is
  byte-for-byte identical (see the browser-parity invariant in `../../CLAUDE.md`).
- **Hosts without a CSS parser** (Node.js, Deno, edge runtimes) — they load this
  module instead of binding the native library.

## Contract
CSSON never writes its own parsing (see `../../CLAUDE.md`): this build is lexbor
itself, walked by the core. It MUST emit **byte-identical canonical JSON** for the
shared suite in `../../conformance`, and the WASM engine is added there as a
first-class reader alongside the C core, Chrome and Firefox.

## Build outline
1. Build `../../core` for WASM with Emscripten (`emcmake cmake` → `emmake`),
   exporting `csson_to_canonical_json`, `csson_set`, `csson_remove`,
   `csson_patch`, `csson_supported_versions`, `csson_free_string`.
2. Ship `csson.wasm` + a small JS glue module (`cwrap` the six entry points,
   marshal strings, free the returned buffers via `csson_free_string`).
3. Run `../../conformance` against the WASM reader; it must match `expected.json`.

## Versions & platforms
- **Supported CSSON standard versions:** 1 (forwarded to the core; never branched on).
- **Architectures:** architecture-independent (wasm32); one artifact runs everywhere.
