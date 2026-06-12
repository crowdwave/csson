# CSSON rebuild on QuickJS + csstree — risk-driven plan

**The bet:** replace the lexbor/C reference core with **one JavaScript implementation**
(csstree for CSS + CSSON logic) running in **QuickJS**, behind the *same* C ABI
(`csson.h`), also compilable to **WASM**. One implementation for every environment;
`@property` validation for free.

This document's job is **not** to cheer the idea — it is to find every way it can
fail and to gate the rebuild behind **go/no-go spikes** that prove it can do
everything CSSON needs *before* we throw away a working C core.

> Status: planning, on branch `rebuild/quickjs-csstree`. The `main` C core stays
> the shipping implementation until every Phase-0 spike is green.

---

## 1. Target architecture

> **Engine split (refined after nesting research — `spikes/nesting/`):**
> **PostCSS** for parse + comment-preserving edits (nests CSSON's bare type
> selectors natively = the browser; `raws`/offsets give clean source-preserving
> edits → **resolves S1's nesting caveat and R4**), and **csstree's `lexer.match`**
> for `@property` (S2-proven browser parity). Both MIT, pure JS, QuickJS-embeddable.

```
libcsson  =  QuickJS (C, ~490 KB native / ~720 KB wasm)
           +  PostCSS (parse/edit) + csstree.lexer (@property) + CSSON.js  (bytecode)
           +  C facade (~200 lines) implementing csson.h
Browser   =  the SAME .wasm (native WebAssembly) — or the JS run directly
Node      =  the SAME JS, native
C/Py/Go/Rust/… = link libcsson (QuickJS native), OR embed the .wasm via wasm2c
```

Measured already (this is real, not estimated): QuickJS-ng native minimal **490 KB**;
wasm **720 KB / 272 KB gzipped**, and it *runs* (`{"x":5}` from `JS_Eval` in WASI).

---

## 2. Can it do everything? — capability map

Every CSSON capability maps to JS; the question is *parity and cost*, not feasibility.

| Capability | Maps to | Confidence |
|---|---|---|
| Read → canonical JSON | csstree parse + walk + `JSON.stringify`(sorted) | ⚠ parity-gated |
| Verbatim scalar (`1e3`, floats) | read source via csstree `loc` | ⚠ parity-gated |
| CSS §3.3 preprocessing | csstree tokenizer (free) | ⚠ verify |
| Integer ≤ 2⁵³ rule | `Number.isSafeInteger` (native) | ✅ |
| Comment-preserving edits | splice at csstree `loc` offsets | ⚠ verify |
| JSON Pointer / Patch | small JS / `fast-json-patch` | ✅ |
| `from_json` | build/serialize via csstree | ✅ |
| **`@property` validation** | `csstree.lexer.match(syntax, value)` | ⚠ parity-gated (the prize) |
| CSSOM handle API | csstree AST is already a tree | ✅ |
| C ABI (`csson.h`) | QuickJS embed + string marshalling | ✅ |
| Small native + wasm | measured: 490 KB / 720 KB | ✅ (heavier than 562 KB today) |

Nothing is *infeasible*. Three things are **parity-gated** and one is a known
**regression** (size, thread-safety). Those are the risks.

---

## 3. Risk register (ranked) — with the spike that kills or clears each

| # | Risk | Sev | Why it could sink the rebuild | De-risk spike (go/no-go) |
|---|---|---|---|---|
| **R1** | **Browser-parity of the read path** | 🔴 Critical | CSSON's prime invariant is byte-identical output to a browser's native `cssRules`. csstree is mdn-data-driven, *not* a browser engine — its value tokenisation / normalisation may not match Chrome/Firefox byte-for-byte. If it can't be made identical, the whole bet fails. | **S1:** run csstree-in-QuickJS over `conformance/v1` fixtures (incl. `edge`: `007`, `-0`, `1e3`, control char, CRLF, invalid UTF-8) and diff against `expected.json` (which *is* the browser output). Must be **byte-identical**. |
| **R2** | **`@property` parity** | 🔴 Critical | The motivation. csstree's verdict must match the browser's `CSS.registerProperty` / `CSSStyleValue.parse`, else "validation" is just a different opinion. | **S2:** drive Chrome (CRI) to register N `@property` cases + value checks; run the same through `csstree.lexer.match` in QuickJS; require agreement (or an explicit, documented allow-list of differences). |
| **R3** | **csstree runs in bare QuickJS** | 🟠 High | csstree's bundle may use web/Node APIs QuickJS lacks (TextEncoder, fetch, fs, console). If the bundle won't load, no engine. | **S3:** `JS_Eval` the csstree IIFE bundle in bare QuickJS; call `parse()` + `lexer.match()`; confirm no missing globals (shim the few that are). |
| **R4** | **Comment-preserving edits via `loc`** | 🟠 High | Edits must splice at exact offsets and keep comments/format. csstree `loc` must be byte-accurate (value start/end, block end). | **S4:** `set`/`remove` on `examples/sample_commented.csson` via csstree `loc` splice; diff that all comments + whitespace survive and the result re-parses. |
| **R5** | **Binary size regression** | 🟠 High | ~1 MB native / ~1 MB wasm vs **562 KB** today. Every binding (wheels, gems, Go binaries) grows. Could be unacceptable for a "lightweight format". | **S5:** build the *full* CSSON bundle (QuickJS + csstree-as-bytecode + logic), measure native + wasm + gzip. Decide acceptable (target ≤ ~1 MB / ≤ ~400 KB gz) or trim csstree to the lexer + needed mdn-data. |
| **R6** | **Thread-safety regression** | 🟠 High | Today the C core is reentrant (no globals) → bindings call concurrently for free. A `JSRuntime` is **not** thread-safe. | **S6:** design + prototype the facade with **thread-local `JSRuntime`** (lazy per-thread) or a runtime pool + lock; measure per-thread init cost (csstree bytecode load). Confirm concurrent calls are safe. |
| **R7** | **CLI startup cost** | 🟡 Med | Each `csson canon` re-inits a runtime + loads csstree. Could add 10–50 ms/call. | **S7:** time cold-start of a QuickJS ctx with csstree-bytecode loaded; if too slow, precompile to bytecode (`qjsc`) and/or snapshot. Library (persistent ctx) is unaffected. |
| **R8** | **Performance on large docs** | 🟡 Med | Interpreter; csstree parse slower than lexbor (~5–20×). The security suite has 64 k-sibling / 20 k-deep inputs. | **S8:** time `canon` on the large/stress fixtures in QuickJS vs the C core; confirm O(N) algorithms (no JS quadratics) and acceptable wall-clock for config-sized data. |
| **R9** | **Memory safety / leaks across the FFI** | 🟡 Med | String marshalling C↔JS + QuickJS GC must not leak; the existing `memcheck` gate must stay green. | **S9:** run the new facade under valgrind + ASan on the full path battery (reuse `tests/memcheck.sh`); 0 leaks. |
| **R10** | **Build/toolchain complexity** | 🟡 Med | Adds a JS bundler + `qjsc` + (for wasm) clang/wasi-sdk/emscripten to a previously pure C/CMake build. | **S10:** stand up a reproducible CMake step: bundle JS → `qjsc` bytecode → `#embed` into the lib; one command, cross-compiles amd64/arm64 + wasm. |
| **R11** | **Determinism / canonical bytes** | 🟡 Med | `JSON.stringify` key order ≠ sorted by default; float/number formatting must match the C/browser canonical exactly. | **S11:** assert the JS canonicaliser emits **the same bytes** as today's `expected.json` (sorted keys, compact, integer rule) on all fixtures — folded into S1. |
| **R12** | **Supply chain / license** | 🟢 Low | New deps: csstree (MIT), QuickJS-ng (MIT), mdn-data (CC0). Clean, but a larger vendored surface. | Vendor pinned versions; record licenses; no transitive npm at runtime (bundle once). |
| **R13** | **Two engines during migration** | 🟢 Low | The browser binding already uses native CSSOM (not csstree). So "parity" = csstree must match native CSSOM, tested by the *existing* harness — a safety net, not a new risk. | Keep `conformance/v1` as the cross-engine gate throughout. |

**The make-or-break is R1+R2: can csstree-in-QuickJS be made byte-identical to the
browser** for both the read path and `@property`. If yes, the rest is engineering.
If no, we abort and keep the C core.

### Risk resolution status (after the spikes + integration prototype)
All risks have been spiked. See `spikes/{s1,s2,nesting,integration}/RESULTS.md`.

| Risk | Status |
|---|---|
| R1 read parity | ✅ byte-identical (PostCSS, all fixtures) |
| R2 @property | ✅ csstree.lexer == browser |
| R3 engines in QuickJS | ✅ PostCSS + csstree both run in QuickJS |
| R4 edits / nesting | ✅ PostCSS native nesting + comment-preserving edits |
| R5 size | ✅ measured 938 KB (~840 KB bytecode) |
| R6 thread-safety | ✅ thread-local runtimes, 400 concurrent calls clean |
| R7 startup | ✅ 44 ms cold |
| R8 perf | ✅ width ~10× (fine for config); deep nesting → clean RangeError bounded by **`JS_SetMaxStackSize`** (engine config, *no* C parser/brace-counting) |
| R9 memory | ✅ valgrind clean |
| N1/N2 edge parity | ✅ UTF-8 + duplicate-key match; ⚠️ **CRLF needs a 5-line §3.3 normaliser in JS** |
| R10/R12 build/supply | ✅ pipeline demonstrated; stub fragility noted |

**Verdict: viable end-to-end, with no hand-rolled parsing anywhere.** The only
code fix needed is a **§3.3 normaliser in JS `coerce`** (~5 lines). Deep-nesting is
handled by an **engine setting** (`JS_SetMaxStackSize` + catch `RangeError`), not a
C parser — a correct C depth guard would require comment/string-aware brace
matching (forbidden hand-rolled parsing), so it is explicitly rejected. The rest
are accepted cost tradeoffs (size ~1.6×, ~10× on pathological width, 44 ms CLI
startup).

---

## 4. Phase 0 — the go/no-go spikes (do these FIRST, on this branch)

No production code until these pass. Each is small and answers one risk.

1. **S1 read-parity** (R1/R11) — csstree-in-QuickJS vs `expected.json` on all
   fixtures, byte-identical. **← the gate; if this fails, stop.**
   **✅ DONE — PASS (3/3 byte-identical in QuickJS).** See `spikes/s1/RESULTS.md`.
   Caveat: csstree doesn't natively parse CSSON's bare nested rules (lags the
   browser on CSS Nesting); recovered via Raw-reparse, but that **complicates the
   edit path** → S4 must pick a nesting strategy (mandate `&`-prefixed selectors,
   or offset-map the reparse). §3.3 edge cases (CRLF/NUL/invalid-UTF-8) not yet
   diffed. **R1 cleared; R4 raised; S4 is now the next gate.**
2. **S3 bundle-loads** (R3) — csstree IIFE runs in bare QuickJS.
3. **S2 @property-parity** (R2) — csstree vs Chrome `registerProperty`/`parse`.
   **✅ DONE — PASS.** `csstree.lexer.match` in QuickJS: 17/17 correct; Chrome
   `CSS.registerProperty`: identical verdicts ⇒ **csstree == browser**. The
   off-the-shelf MIT `csstree/validator` solves `@property` in QuickJS — no Rust
   engine needed. See `spikes/s2/RESULTS.md`. (Representative set; gate the full
   type set against the browser oracle.)
4. **S4 edits** (R4) — comment-preserving `set`/`remove` via `loc`.
5. **S5 size** (R5) — full bundle measured native + wasm + gzip.
6. **S6 threads** (R6) + **S7 startup** (R7) + **S8 perf** (R8) — the cost spikes.

**Exit criterion for Phase 0:** S1, S2, S3, S4 green (correctness) **and** S5–S8
within budget (cost). Only then do we commit to the rebuild.

---

## 5. Phased rebuild (only after Phase 0 is green)

- **P1 — Read path:** CSSON.js read → canonical JSON; pass the full `conformance/v1`
  (C core, Chrome, Firefox **and** the new QuickJS engine all byte-identical).
- **P2 — Edits:** set/remove/patch via `loc` splice; pass `tests/edits.sh`.
- **P3 — `@property` + handle API:** `csson_validate` (the prize) + the CSSOM
  handle API over csstree's AST.
- **P4 — C facade + packaging:** `csson.h` over QuickJS (native) and the `.wasm`
  (wasm2c / wasm3); CMake build (bundle → qjsc → `#embed`); amd64/arm64 + wasm.
- **P5 — Cutover:** the new engine must pass **every existing gate** —
  `conformance/v1/verify.sh`, `ctest` (canon/edits/security/api/sheet/memcheck) —
  byte-for-byte. Flip the default; keep the C core tagged for one release.

The existing test suite **is** the acceptance spec: the rebuild is "done" when it
passes the same conformance + security + memcheck gates the C core passes today,
with identical bytes.

---

## 6. Kill criteria (when we abort and keep the C core)

- **S1 fails irreparably** — csstree can't be made byte-identical to the browser
  read path even reading verbatim via `loc`. (Most likely killer.)
- **S2 shows wide, unfixable `@property` divergence** from browsers.
- **S5 size** blows past ~1.5 MB native / ~600 KB gz with no viable trim.
- **S6 thread-safety** can't be made safe without unacceptable per-call cost.

Any one of these → stop, keep lexbor/C on `main`, and instead pursue the *narrow*
win: keep the C core and bind a real engine **only** for `@property`
(lightningcss/wasm) — the option we already scoped.

---

## 7. What we keep regardless

The C ABI shape (`csson.h`), the JSON Pointer/Patch semantics, the canonical-JSON
contract, and the **entire test/conformance/security/memcheck harness**. Those are
engine-independent and are exactly what makes a from-scratch engine swap safe to
attempt: we can prove the rebuild correct by re-running the existing gates.
