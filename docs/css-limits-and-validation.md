# CSS standards limits that apply to CSSON (and how CSSON validates against them)

CSSON uses only a small slice of CSS: **style rules** (nested), **custom
properties** as fields, scalar **declaration values**, and optional `@property`.
This collects the limits the CSS standards (and the real engines) place on *that
slice*, what CSSON does about each, and where a limit is a **browser-parity risk**
(a place engines could diverge, which would break CSSON's byte-identical invariant).

## Summary

| # | Limit | Spec position | Blink (Chrome) | Gecko (Firefox) | lexbor | CSSON handling / parity risk |
|---|---|---|---|---|---|---|
| 1 | **Input preprocessing**: CR, CRLF, FF → LF; NUL & ill-formed bytes → U+FFFD | **Mandated**, CSS Syntax L3 §3.3 | applies | applies | applies internally | **CSSON now applies it** to the verbatim token (`preprocess()`); was a parity bug (S4) until fixed |
| 2 | **Default encoding** UTF-8; order: BOM → HTTP charset → `@charset` → UTF-8 | CSS Syntax L3 §3.2 / §"Determine the fallback encoding" | yes | yes | byte-oriented (caller decides) | CSSON files are UTF-8; output guaranteed UTF-8 after fix #1. **Parity risk** if a file is served with a non-UTF-8 charset — mandate UTF-8 (below) |
| 3 | **Custom-property name** case-sensitive; ident code points + escapes | CSS Variables L1 §2 | as spec | as spec | as spec | CSSON keys are these names minus `--`; canonical JSON keys are case-sensitive (matches) |
| 4 | **Custom-property value**: any token sequence (no `;`/top-level `!`/unmatched brackets); **no length limit in spec** | CSS Variables L1 §3 | stores large values | stores large values | stores | CSSON scalar = one such value; `set` validates it's a single scalar |
| 5 | **`var()` expansion** size cap | **implementation-defined** | 65,536 tokens | 1 MB | n/a | CSSON does **not** use `var()`, so this never applies (we read authored tokens, not substituted) |
| 6 | **Numbers** are IEEE-754 doubles; integers beyond 2^53 lose precision | CSS Values L4 (math is IEEE-754) | double | double | double | CSSON emits a JSON **number only for canonical integers ≤ 2^53−1**, else a string — matches `Number.isSafeInteger`, so all engines agree |
| 7 | **Nesting depth**: no spec limit; no hard engine limit (memory-bound) | CSS Nesting L1 (silent) | unbounded (until OOM/stack) | unbounded | unbounded (no depth cap) | **CSSON caps its own walk at `CSSON_MAX_DEPTH` 512** and errors cleanly (a DoS guard, not a CSS limit) |
| 8 | **Selector specificity** fields are bounded (A/B/C) | Selectors L4 | clamps | clamps | `SP_*_MAX` = 2^28/2^27/2^18/2^9−1 | Irrelevant: CSSON selectors are type tags, never matched/cascaded |
| 9 | **Comments** don't nest; unterminated comment runs to EOF | CSS Syntax L3 §4 (tokenizer) | as spec | as spec | as spec | Drove security finding S3 (`set` round-trip guard) and the edit-path comment-aware brace scan |
| 10 | **Stylesheet size / rule count**: no spec cap | — | memory-bound | memory-bound | memory-bound | CSSON CLI caps input at 256 MiB (`CSSON_MAX_INPUT`); read is O(N) after the S5 fix |
| 11 | **`@property` `syntax`** grammar; descriptors required | CSS Properties & Values API L1 | as spec | as spec | parsed | `@property` is advisory in CSSON; not required for structure |

## Notes per limit

1. **Input preprocessing (the big parity item).** CSS Syntax §3.3 *mandates*
   normalising newlines and replacing NUL/invalid bytes with U+FFFD *before*
   tokenization, so every conforming engine sees the same code points. Because
   CSSON reads the **verbatim source bytes** (to keep `1e3`/float precision), it
   must reproduce this step itself or it diverges from the browser — which it did
   (security finding **S4**). `preprocess()` now performs exactly §3.3 (CR/CRLF/FF
   → LF; NUL & ill-formed UTF-8 → U+FFFD via WHATWG maximal-subpart), leaving
   digits untouched. Verified byte-identical core ↔ Chrome ↔ Firefox.

2. **Encoding.** A CSS stylesheet's encoding is determined by BOM, then the HTTP
   `Content-Type; charset`, then an `@charset` rule, else **UTF-8**. To keep
   byte-parity, a CSSON file should be **UTF-8 with no `@charset` and no BOM**
   (see the mandate recommendation in `docs/` discussion). A document served as a
   different charset could be decoded differently by a non-browser reader.

4–5. **Values.** The spec deliberately sets **no length limit** on a custom
   property value; the only implementation cap is on `var()` *expansion*
   (Chromium/WebKit 65,536 tokens; Gecko 1 MB). CSSON never substitutes `var()`,
   so that cap is moot — CSSON reads the authored token verbatim.

6. **Numbers.** CSS math is IEEE-754; integers above 2^53 cannot be represented
   exactly as doubles. CSSON's integer rule (`is_canonical_int` + `int_in_safe_range`
   ≤ 9007199254740991) keeps numbers in the range every engine and JSON consumer
   agrees on; larger or non-canonical numerics stay verbatim strings.

7. **Nesting.** Neither CSS Nesting L1 nor the engines define a maximum depth; it
   is bounded only by memory/stack. lexbor parses 60k-deep input fine. CSSON's
   `CSSON_MAX_DEPTH` (512) is **our** guard against stack overflow in the JSON
   walk (finding H1), not a CSS limit — deeper input is rejected, not crashed.

## What CSSON validates / enforces today

- **§3.3 preprocessing** applied on read (parity + always-valid-UTF-8 output).
- **Integer range** ≤ 2^53−1 (canonical) → JSON number, else string.
- **Depth** ≤ 512 on the walk; over-deep input errors cleanly.
- **Input size** ≤ 256 MiB at the CLI.
- **Scalar/edit validation**: `set` value must be a single scalar and must
  round-trip; patch keys/types must be identifier-safe.

## Recommended additional mandates (spec candidates)

- **Encoding:** a `.csson` file MUST be UTF-8, MUST NOT use `@charset`, SHOULD NOT
  start with a BOM, and SHOULD be served `Content-Type: text/css; charset=utf-8`.
  This removes the only realistic encoding parity risk (#2).
- **No `var()` / `env()` in values** (they introduce substitution and the
  engine-specific expansion caps of #5); CSSON values are literal scalars.
- **Custom-property names** SHOULD restrict to `[A-Za-z0-9_-]` plus non-ASCII (the
  same `is_safe_name` set the editor enforces), so a name can never carry CSS
  structure and round-trips identically everywhere.

## Sources

- [CSS Syntax Module Level 3](https://www.w3.org/TR/css-syntax-3/) — §3.2 decode, §3.3 preprocessing, §4 tokenizer/comments.
- [CSS Custom Properties for Cascading Variables Module Level 1](https://www.w3.org/TR/css-variables-1/) — name/value rules; `var()` expansion limit note.
- [CSS Values and Units Module Level 4](https://www.w3.org/TR/css-values-4/) — numeric values / IEEE-754.
- [MDN: Numeric data types](https://developer.mozilla.org/en-US/docs/Web/CSS/Guides/Values_and_units/Numeric_data_types) and [`<number>`](https://developer.mozilla.org/en-US/docs/Web/CSS/number).
- [MDN: `@charset`](https://developer.mozilla.org/en-US/docs/Web/CSS/@charset) and [W3C i18n: Declaring character encodings in CSS](https://www.w3.org/International/questions/qa-css-charset).
- [MDN: `--*` custom properties](https://developer.mozilla.org/en-US/docs/Web/CSS/--*) and [csswg-drafts #5510 (var size limit)](https://github.com/w3c/csswg-drafts/issues/5510).
- [web.dev: CSS Nesting](https://web.dev/learn/css/nesting); [MDN: `&` nesting selector](https://developer.mozilla.org/en-US/docs/Web/CSS/Reference/Selectors/Nesting_selector).
- lexbor (vendored): `css/selectors/selector.h` specificity caps; no nesting-depth cap.
