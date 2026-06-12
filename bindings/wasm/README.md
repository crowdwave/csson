# CSSON for WebAssembly

The portable front door: the CSSON core (QuickJS-ng + PostCSS + csstree + our
TypeScript, via the C facade `libcsson`) compiled to a **WASI reactor** module.

| | |
|---|---|
| **Environment** | WebAssembly (browsers, Node.js, Deno, any WASI preview1 host) |
| **Front door** | `core/src/facade.c` + QuickJS-ng + the JS bundle → `csson.wasm` |
| **Mechanism** | wasi-sdk clang builds a reactor exporting the C ABI (`csson.h`); a small JS host (`csson.mjs`) marshals strings and frees returned buffers |
| **Distribution** | the raw `csson.wasm` + `csson.mjs` glue (npm `@csson/wasm` planned) |

## Why WASM exists
Two jobs no other front door can do everywhere:
- **In-browser editing.** The native CSSOM exposes no source byte offsets and
  editing via `cssText` strips comments, so `set`/`remove`/`patch` run the *same*
  QuickJS+TS core compiled to WASM — byte-for-byte identical to the native core
  (the browser-parity invariant in `../../CLAUDE.md`).
- **Hosts without a native build** (Node.js, Deno, edge runtimes) load this
  module instead of binding the native library.

## Build
```sh
# needs wasi-sdk (clang + wasm32-wasi sysroot); override with WASI_SDK=/path
bindings/wasm/build.sh        # → bindings/wasm/csson.wasm  (~1.4 MB)
```
The script bundles the TypeScript (`core/tools/build-bundle.mjs`), compiles
QuickJS + `facade.c` for `wasm32-wasi`, and links a reactor exporting the full
ABI plus `malloc`/`free` (so the host can place input bytes and release returned
strings). QuickJS-ng has first-class WASI support; only `dtoa.c` uses
setjmp/longjmp, which wasi-sdk provides.

## Use (Node)
```js
import * as csson from "./csson.mjs";
csson.toCanonicalJson(text);     // canonical JSON
csson.get(text, "/a/0/b");       // one value at a JSON Pointer
csson.setJson(text, "/port", 9); // comment-preserving edit → new source
csson.validate("<integer>", "5");// @property check
```
`csson.mjs` loads the module with Node's built-in `node:wasi`; in a browser, load
`csson.wasm` with any WASI preview1 shim and reuse the same marshaling.

## Conformance
The WASM core is a first-class reader in `../../conformance/v1` (alongside the C
core, Chrome and Firefox). It MUST emit **byte-identical canonical JSON** for the
shared suite — verified on every fixture (`readers/wasm/canon_wasm.js`).

## Versions & platforms
- **Supported CSSON standard versions:** 1 (forwarded to the core; never branched on).
- **Architecture:** `wasm32-wasi`; one artifact runs everywhere.
