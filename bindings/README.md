# CSSON language bindings

Use CSSON — structured data that is also valid CSS — from your language. Each
binding drives the **same** CSSON core (QuickJS + PostCSS + csstree), so every
one produces byte-identical canonical JSON (the browser-parity invariant).

| Binding | Mechanism | Needs | Runtimes |
|---|---|---|---|
| **[javascript](javascript/)** (JS/TS) | `csson.wasm` (WebAssembly) | nothing native | Node.js, Deno, browsers |
| **[python](python/)** | `ctypes` FFI → `libcsson.so` | the native shared lib | CPython ≥ 3.9 |

Both expose the same surface: `parse`/`loads`, `get`, `set`, `remove`, `patch`,
`stringify`/`dumps`, `validate`, `version`.

- **[wasm/](wasm/)** is not a binding — it's the build that produces the portable
  `csson.wasm` core the JavaScript binding loads (`bash bindings/wasm/build.sh`).

## Which to pick
- **JS/TS** — zero native build; one portable `.wasm` runs in Node, Deno and the
  browser. In the browser, editing must go through this core (the native CSSOM
  strips comments and exposes no byte offsets).
- **Python** — in-process via `ctypes` (stdlib, no third-party deps); needs the
  native `libcsson` shared library, built from `core/` (`cmake --build core/build
  --target csson_shared`) or shipped beside the module.

See each binding's README for install steps, a runnable example, and tests.
