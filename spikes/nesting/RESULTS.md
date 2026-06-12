# Nesting — alternative-parser research (resolving the S1 gap)

**Problem (from S1):** css-tree 3.2.1 doesn't parse CSSON's **bare type-selector
nesting** (`cssonv1 { department { … } }`) — browsers/lexbor do, but css-tree dumps
it to `Raw`. The Raw-reparse workaround read correctly but lost source offsets,
putting the **edit path (R4)** at risk. Goal: find a JS parser with the **best
standards compatibility** that nests natively and supports source-preserving edits.

## Empirical test — every whole-document JS parser, vs the browser

Input: `cssonv1 { --org; department { --name; team { --lead; } } }` (3 nested
levels, bare type selectors). Target = the browser's structure.

| Parser | Nesting | Notes |
|---|---|---|
| **PostCSS** 8.5 | ✅ **native** | `cssonv1 {--org, department {--name, team {--lead}}}` — exact 3-level match |
| @adobe/css-tools 4.5 | ❌ | nested rule mis-parsed as `@rule` |
| cssom 0.5 | ❌ | `Unexpected }` — can't parse nesting |
| gonzales-pe 4.3 | ❌ | rejects the block (css syntax) |
| css-tree 3.2.1 | ❌ | `[Declaration, Raw]` (the S1 gap) |
| @csstools/css-parser-algorithms | n/a | low-level component-value parser; no rule tree out of the box |

**PostCSS is the only practical JS rule-tree parser that nests CSSON natively.**

## PostCSS clears every requirement

1. **Native nesting** — matches the browser, no workaround.
2. **Byte-identical to the browser** — a PostCSS-based reader reproduces
   `expected.json` on **all 3 conformance fixtures** (incl. nested `csson_v1` and
   the `edge` cases):
   ```
   IDENTICAL  csson_v1
   IDENTICAL  fixtures/numeric
   IDENTICAL  fixtures/edge
   ```
3. **Comment-preserving edits (solves R4)** — nodes carry `source.start/end`
   offsets and `raws`; mutate a value → `toString()` keeps comments + formatting:
   ```
   --org: "Beta";          /* trailing comment */   ← comment + layout preserved
   ```
   This is PostCSS's core purpose, and it's cleaner than manual byte-splicing.
4. **Runs in QuickJS** — bundled to **137 KB** (esbuild, parser+stringify) after
   stubbing 4 node-only deps it never calls for string parse (`picocolors`, `fs`,
   `path`, `url`); parsed the nested doc in `qjs` → `{"rules":3,"decls":3}`.

## Recommended engine split (refines the rebuild)

| Job | Library | Why |
|---|---|---|
| **Parse + edit** | **PostCSS** (MIT) | native nesting = browser; raws/offsets ⇒ comment-preserving edits; runs in QuickJS |
| **`@property` validate** | **csstree `lexer.match`** (MIT) | S2-proven browser parity; standalone (takes strings, no document parser) |

Both pure JS, MIT, QuickJS-embeddable. Size ≈ QuickJS ~490 KB + PostCSS ~137 KB +
csstree-lexer (trimmed) ⇒ ~0.8–0.9 MB. This **resolves S1 (nesting) and R4 (edits)
in one move** while keeping the S2 `@property` win.

## Standards-compatibility note (honest)

PostCSS is **lenient/forgiving**, not a strict spec implementation — it matched the
browser byte-for-byte on CSSON documents (what matters; gated by conformance), but
on *malformed* input it may diverge. The strictest options are
`@csstools/css-parser-algorithms` (spec-faithful but low-level, no rule tree) and
**lightningcss-wasm** (Rust, most spec-current) — keep the latter in reserve if a
parser-differential ever shows up against the browser oracle.

## Repro
`spikes/nesting/pcread.mjs` (PostCSS reader) + `run_parsers.mjs` (the comparison);
`postcss@8.5.15`; bundled with esbuild + 4 stub shims; run in `quickjs-ng` `qjs`.
