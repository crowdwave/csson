# S1 — read-path browser-parity spike (the go/no-go gate)

**Question:** does a CSSON reader built on **csstree, running in QuickJS**, produce
**byte-identical** canonical JSON to the browser (= the committed `expected.json`)?

## Verdict: ✅ PASS — but with one significant caveat

Running `spikes/s1/read.mjs` (csstree parser-only) inside **QuickJS-ng**:

```
S1 GATE: parser-only csstree reader in QuickJS vs expected.json
  IDENTICAL  csson_v1
  IDENTICAL  fixtures/numeric
  IDENTICAL  fixtures/edge
  -> QuickJS engine: 3/3 byte-identical to expected.json
```

All three conformance fixtures — including the `edge` cases (`007`, `-0`, `1e3`,
oversized integers, control char) — reproduce the browser/C-core bytes exactly,
**in the actual QuickJS engine**, not just Node. The core hypothesis holds: a JS
implementation can match the browser byte-for-byte. **The bet is alive.**

Engine/size observed: parser-only JS bundle **115 KB** (smaller as bytecode);
QuickJS native ~490 KB → a **read-path** CSSON-on-QuickJS ≈ ~600 KB, comparable to
today's 562 KB C lib. (`@property` would add the lexer + mdn-data.)

## The caveat (a new risk S1 discovered) 🟠

**csstree does not parse CSSON's bare nested rules** the way browsers do.
css-tree 3.2.1's `Block.parse` only treats a nested rule as a rule when it starts
with `&` (`if (isStyleBlock && this.isDelim(AMPERSAND))`). CSSON uses bare type
selectors (`department { … }`), which **modern Chrome/Firefox/lexbor nest** but
css-tree dumps into a single `Raw` node. So stock csstree is *behind the browser*
on CSS Nesting.

The spike recovers parity by **re-parsing each `Raw` block as a stylesheet** — and
that produces byte-identical output. But:

- ⚠️ Re-parsed `Raw` is parsed **in isolation**, so its node offsets are relative to
  the substring, **not the original document**. That breaks the
  splice-at-`loc`-offset model the **edit path** (S4) depends on.
- ⚠️ Arguably the workaround is us doing parsing css-tree won't — against the prime
  directive (rules dropped for this exercise, but noted).

### Mitigations to evaluate before committing
1. **Mandate `&`-prefixed nested selectors** in CSSON (`& department { … }`). Then
   csstree parses nesting natively (clean offsets, no Raw-reparse), the browser
   still nests them, and the existing `seltype` already strips a leading `&`. This
   is the cleanest fix but a **format change**.
2. **Use a nesting-capable JS parser** (e.g. lightningcss-wasm) — but that leaves
   the "pure JS in QuickJS" story.
3. **Map offsets** from the re-parsed Raw back to the document (bookkeeping).

## Consequence for the plan
- **R1 (read parity): CLEARED** for the conformance fixtures.
- **New/raised risk → R4 (edits):** the nesting workaround makes
  splice-at-`loc` harder. **S4 is now the next gating spike**, and it must decide
  the nesting strategy (mitigation 1 vs 3) before P1/P2.
- **Not yet tested (R1 extension):** §3.3 edge cases (CRLF, NUL, invalid UTF-8)
  — the fixtures are clean ASCII; csstree's tokenizer behavior there must still be
  diffed against the browser.

## Repro
`spikes/s1/read.mjs` + `css-tree@3.2.1`, bundled with esbuild
(`--format=iife --platform=neutral`, parser-only entry → no lexer/mdn-data, no
`createRequire`), run in `quickjs-ng` `qjs`. Inputs inlined; output diffed against
`conformance/v1/*expected.json`.
