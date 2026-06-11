# CSSON for Node.js

Status: **planned** (scaffold). Thin wrapper over the C core (`libcsson`, on
liblexbor) in `../../core`.

| | |
|---|---|
| **Environment** | Node.js (server-side JavaScript — no DOM/CSSOM) |
| **Front door** | WASM (`../wasm`), primary · C ABI (N-API) optional fast path |
| **Mechanism** | load `@csson/wasm` and wrap canon/set/remove/patch; optional N-API addon over `libcsson` for native speed |
| **Distribution** | npm `csson` (Node entry; the browser entry is `../../packages/typescript`) |

## Why Node needs its own binding
The browser package (`../../packages/typescript`) reads through the browser's
**built-in CSSOM** — but Node has no CSSOM. So Node uses lexbor compiled to WASM
(`../wasm`) as its CSS parser. Same engine as the native core ⇒ byte-identical
output, and unlike the browser it also gets `set`/`remove`/`patch` for free
(the WASM build carries the edit path).

## Contract
CSSON never writes its own parsing (see `../../CLAUDE.md`): this binding reads via
a real CSS parser (lexbor-via-WASM) and only walks the rule tree. It MUST emit
**byte-identical canonical JSON** for the shared suite in `../../conformance` —
the same bytes as the browser CSSOM reader and the C core.

## Build outline
1. Build `../wasm` (lexbor + core → `.wasm`).
2. Wrap it as an npm package: `toCanonicalJson(text)`, `set`, `remove`, `patch`,
   matching the TypeScript API surface; provide ESM + CJS entry points.
3. Run `../../conformance` fixtures under Node; output must match `expected.json`.

## Versions & platforms
- **Supported CSSON standard versions:** 1 (forwarded to the core; never branched on).
- **Architectures:** wasm path is architecture-independent; the optional native
  N-API addon ships amd64 + ARM64 per `../../core/PLATFORMS.md`.
