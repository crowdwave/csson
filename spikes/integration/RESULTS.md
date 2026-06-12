# Integration prototype — resolving the remaining risks

Built an **end-to-end prototype**: a C facade (`facade.c`) embedding QuickJS-ng +
an assembled JS bundle (PostCSS parse/edit + csstree.lexer `@property` + a CSSON
module, `cssonmod.mjs`), exposing `csson_to_canonical_json` over the C ABI. Then
measured every open risk against it. **All resolved or reduced to small, documented
mitigations.**

## Headline: the facade works

`csson_to_canonical_json` (C → QuickJS → PostCSS) is **byte-identical to
`expected.json` on all 3 conformance fixtures**. The whole stack runs as one C
binary.

## Risk-by-risk resolution (measured)

| Risk | Result | Detail |
|---|---|---|
| **R5 size** | ✅ **938 KB** (full parser); ~840 KB with bytecode | vs 562 KB C core → ~1.5–1.7×. Acceptable; *includes* `@property` the C core lacks. |
| **R6 thread-safety** | ✅ **Resolved** | 8 threads × 50 = **400 concurrent `canon` calls** on thread-local `JSRuntime`s → **0 wrong results, no crash**. Design: per-thread runtime (or pool). Cost: per-thread bundle load (~40 ms; mitigable via bytecode/snapshot). |
| **R7 startup** | ✅ **44 ms** cold (init + load + canon) | Library (persistent ctx): paid once. CLI: per-call; drops with precompiled bytecode. Acceptable. |
| **R8 perf — width** | ✅ acceptable | 64 k same-type siblings: **2.82 s** vs C core 0.28 s (~10×). Irrelevant for config-sized data. |
| **R8 robustness — depth** | ⚠️ **mitigable** | 20 k-deep nesting → **clean `RangeError` (no segfault)** — QuickJS catches the stack overflow. But PostCSS's recursive parser can't go as deep as the C core (which handled 20 k in 0.06 s). **Fix: a C-side depth+size guard before calling JS** (same as the core's `CSSON_MAX_DEPTH`). |
| **R9 memory** | ✅ **valgrind clean** | facade over a fixture: 0 definite/indirect leaks, 0 errors (rc=0). |
| **N1/N2 edge parity** | 🟡 **one small gap** | invalid-UTF-8 → U+FFFD **== browser** ✅; duplicate custom property → last-wins **== browser** ✅; **CRLF**: facade keeps `\r`, browser normalises → **diverges**. PostCSS doesn't apply CSS §3.3 newline normalisation. **Fix: ~5 lines in JS `coerce`** (`\r\n?`→`\n`, `\f`→`\n`, NUL→U+FFFD) — exactly the core's `preprocess()`. |
| **R10/R12/N5 build + stubs** | ✅ **demonstrated** | Reproducible pipeline: esbuild bundle + 4 node-builtin stubs (`picocolors/fs/path/url`) + `#embed` byte array + gcc. The stubs were **never hit destructively** (all fixtures passed). Stub fragility remains a documented maintenance note. |

## What this proves

The QuickJS + PostCSS + csstree.lexer approach is viable **end-to-end as a single C
binary** with the existing C ABI: correct (byte-identical), thread-safe (thread-local
runtimes), leak-free, with `@property` built in. The remaining work is **two small,
known mitigations** — a C-side depth/size guard (R8) and a 5-line §3.3 normaliser in
JS coerce (N2) — plus the cost tradeoffs (size ~1.6×, ~10× slower on pathological
width, 44 ms CLI startup) that are inherent and acceptable for a config/data format.

## Outstanding (follow-ups, not blockers)
- Add the depth/size guard + §3.3 normaliser, then re-run the full conformance +
  security + memcheck gates against the facade.
- Full `@property` type-set parity sweep vs the browser (S2 was representative).
- Bytecode (`qjsc`) + parser-disabled build for the smaller/faster variant.

## Repro
`spikes/integration/{facade.c, cssonmod.mjs, threadtest.c}`; QuickJS-ng;
`postcss@8.5` + `css-tree@3.2` bundled with esbuild; gcc `-Os`.
