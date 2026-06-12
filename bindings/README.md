# CSSON language bindings

Use CSSON — structured data that is also valid CSS — from your language. Each
binding drives the **same** CSSON core (QuickJS + PostCSS + csstree), so every
one produces byte-identical canonical JSON (the browser-parity invariant).

**Nothing to compile** — each binding ships its prebuilt artifact, so you just
import and use it.

| Binding | Mechanism | Ships | Runtimes |
|---|---|---|---|
| **[javascript](javascript/)** (JS/TS) | `csson.wasm` (WebAssembly) | the prebuilt `dist/` + portable `csson.wasm` | Node.js, Deno, browsers |
| **[python](python/)** | `ctypes` FFI → `libcsson.so` | a prebuilt `libcsson.so` (Linux x86-64) | CPython ≥ 3.9 |

Both expose the same surface: `parse`/`loads`, `get`, `set`, `remove`, `patch`,
`stringify`/`dumps`, `validate`, `version`.

- **[wasm/](wasm/)** is not a binding — it's the build that produces the portable
  `csson.wasm` core (committed; the JavaScript binding loads it). Run
  `bash bindings/wasm/build.sh` only to regenerate it from source.

## Which to pick
- **JS/TS** — fully portable: one `.wasm` runs everywhere with no native build, in
  Node, Deno and the browser. In the browser, editing must go through this core
  (the native CSSOM strips comments and exposes no byte offsets).
- **Python** — in-process via `ctypes` (stdlib, no third-party deps). The prebuilt
  `libcsson.so` is Linux x86-64; on other OSes drop a matching prebuilt library
  beside the module or build one (`cmake --build core/build --target csson_shared`).

See each binding's README for install steps, a runnable example, and tests.
