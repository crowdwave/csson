# CSSON C code — bug & flaw analysis

Adversarial multi-technique sweep of the project's **C code**, ranked by severity.
Findings marked **[confirmed]** were reproduced against the built binary
(`core/build/csson`); **[by-inspection]** are derived from code reading with high
confidence.

> **Status: all findings below are FIXED.** Every item (C1–C2, H1–H4, M1–M4,
> L1–L5) was resolved in `core/src/csson_core.c` / `core/cli/csson.c`. See
> **[Resolution](#resolution)** for the per-finding fix and how it is locked in by
> the test suite. This document is retained as the record of what was found.

## Scope (files analysed)

> Historical note: at the time of this analysis the core was a single
> `core/src/csson_core.c`. It has since been split into layered modules
> (`mem`, `buffer`, `json`, `pointer`, `stylesheet`, `scalar`, `textedit`,
> `serialize`, `read`, `edit`, `patch`, `version`); the function references below
> name where each issue lived in the monolith.

- `core/src/csson_core.c` — the `libcsson` reference core (read + edit + patch)
- `core/cli/csson.c` — the `csson` CLI
- `core/include/csson.h` — the C ABI
- `conformance/v1/readers/c/csson_c.c` — independent C/lexbor conformance reader
  (shares the read-path algorithm with the core, so it inherits the read-path bugs)

**Out of scope (vendored, trusted):** `core/third_party/yyjson.*` (fuzzed) and
lexbor (fetched). Their robustness is assumed; CSSON-authored C is the target.

## Method
Each technique from the brief was applied as a separate pass over every file, then
the whole sweep was repeated until new passes stopped surfacing distinct issues
(≈4 productive passes; later passes only re-derived known findings → diminishing
returns reached). The techniques that found nothing, and why, are listed at the end
so the absence is deliberate, not skipped.

---

## Severity summary

| # | Severity | Finding | Location |
|---|---|---|---|
| C1 | **Critical** | Read path emits **invalid JSON** for leading-zero integers (`007`) — also breaks browser-parity | `coerce` / `node_json` |
| C2 | **Critical** | Read path emits **invalid JSON** for strings with control chars / unescaped `"`/`\` (no JSON string escaping) — also breaks browser-parity | `quote` / `coerce` / `node_json` |
| H1 | **High** | **Stack-overflow DoS** on deeply nested input (unbounded recursion) | `node_json`, lexbor parse |
| H2 | **High** | **Injection**: `csson_set` writes the raw value verbatim → structural CSS injection | `csson_set` |
| H3 | **High** | `csson_remove` **corrupts structure** when the field/node is not alone on its line | `csson_remove` |
| H4 | **High** | Hand-rolled brace/quote matcher is **comment-unaware** → wrong span → corruption (also violates the prime directive) | `csson_remove`, `blk_close` |
| M1 | **Medium** | JSON **object keys are not escaped** (same omission as C2, for keys) | `node_json` |
| M2 | **Medium** | Huge integers emitted verbatim → browser-parity divergence + downstream precision loss | `coerce` |
| M3 | **Medium** | Empty/root pointer (`""`/`/`) silently **deletes the whole document** | `resolve` / `csson_remove` |
| M4 | **Medium** | Selector/key **silently truncated at 255 bytes** → wrong node type | `sbcb` / `ntype` |
| L1 | **Low** | JSON Pointer silently truncated at 64 segments | `split_pointer` |
| L2 | **Low** | lexbor `create`/`init` return values unchecked (OOM → crash) | `parse_root`, readers |
| L3 | **Low** | `csson_set(value=NULL)` silently empties the value instead of erroring | `csson_set` |
| L4 | **Low** | `uint64 > INT64_MAX` patch value mis-serialised via `%lld` | `ser_scalar` |
| L5 | **Low** | Unchecked `size_t` growth arithmetic (`(n+l+1)*2`, `sizeof*(n+1)`) — theoretical overflow | `bput`, `node_json` |

---

## Critical

### C1 — Leading-zero integers produce invalid JSON (and diverge from the browser) **[confirmed]**
**Where:** `coerce` (csson_core.c) + the integer branch of `node_json`; same in `csson_c.c`.
**Techniques:** Boundary/Equivalence Partitioning, Protocol Conformance (JSON), Regression/Parity, Adversarial Input.

`coerce` classifies any `-?[0-9]+` token as an integer and emits the **verbatim digits** as a JSON number. Leading zeros are preserved, so the output is not valid JSON:
```
input : cssonv1{ --x: 007; }
output: {"x":007}          # python json.loads → JSONDecodeError
```
The browser reader instead does `parseInt("007",10)` → `7`, so the two engines **disagree** (violating the non-negotiable browser-parity invariant) *and* the C output is syntactically invalid JSON. Any consumer (including the core's own `op_test`/`move`/`copy`, which re-parse the canonical JSON with yyjson) will reject it.
**Fix:** normalise integer output (reject/ξ leading zeros, or emit the parsed value), and define the canonical form in the spec so all engines agree.

### C2 — Strings/identifiers are not JSON-escaped → invalid JSON (and divergence) **[confirmed]**
**Where:** `quote` + `coerce` + the value memcpy in `node_json`; same in `csson_c.c`.
**Techniques:** Information-Flow/Taint, Adversarial Input, Protocol Conformance, Canonicalization.

`quote()` wraps a value in `"` but performs **no JSON escaping**. A value containing a control character, `"`, or `\` yields invalid JSON:
```
input : cssonv1{ --s: "a<TAB>b"; }
output: {"s":"a<TAB>b"}     # raw U+0009 in a JSON string → invalid
```
Identifier values containing `"`/`\`, and quoted CSS strings using CSS-only escapes (e.g. `\A `), also emit invalid JSON. The browser builds output via `JSON.stringify`, which escapes correctly — so again **invalid JSON + engine divergence**. (Note the patch path's `ser_scalar` escapes `"`/`\` but still not control chars, so it has a narrower form of the same bug.)
**Fix:** emit values through a proper JSON string-escaper (escape `"`, `\`, and U+0000–U+001F) for both keys and values.

---

## High

### H1 — Stack-overflow DoS on deeply nested input **[confirmed]**
**Where:** `node_json` recursion (read), and lexbor's own parse of deep nesting.
**Techniques:** Adversarial Input/Fuzzing, DoS/Resource Exhaustion, Stress Pattern.

A document nested ~200k deep crashes the process:
```
cssonv1{ a{ a{ … }} }   (depth 200000)  → AddressSanitizer: DEADLYSIGNAL (SIGSEGV)
```
`node_json` recurses once per nesting level with no depth limit; lexbor's recursive parse compounds it. Untrusted `-csson.css` (the whole point of the browser/edge use case) can crash any host linking `libcsson`.
**Fix:** impose a nesting-depth limit (return an error past N levels) in the walk; consider an explicit stack.

### H2 — `csson_set` injects raw value bytes (structural injection) **[confirmed]**
**Where:** `csson_set` (csson_core.c) and the CLI `set` verb.
**Techniques:** Injection Surface, Trust Boundary, Information-Flow/Taint.

`csson_set` splices the caller's `value` into the source **verbatim**, with no validation/escaping:
```
csson set f /x '2;} pwned{--z:9'
→  cssonv1{ --x: 2;} pwned{--z:9; }      # broke out of the declaration, injected a node
```
A value containing `;`, `}`, `{`, or `/* */` restructures the document. Unlike the patch path (which serialises via `ser_scalar`), `csson_set` trusts the value entirely. With any untrusted value this is a structural-injection vulnerability.
**Fix:** route `csson_set` through the same scalar serialiser/validator as the patch path (quote+escape strings, validate numeric/identifier forms), or reject values that don't match a scalar grammar.

### H3 — `csson_remove` corrupts structure for non-line-isolated fields/nodes **[confirmed]**
**Where:** `csson_remove` (both the field and node branches).
**Techniques:** Equivalence Partitioning, Completeness/Error-Path, Semantic-Gap.

Removal extends the span to the **whole line** (`while (a>0 && src[a-1] != '\n') a--`), assuming one declaration/rule per line. For compact input it deletes preceding content:
```
input : cssonv1{\n  point { --x: 1; --y: 2; }\n}
rm /point/0/x →
cssonv1{
--y: 2; }
}                      # `point {` and indentation destroyed; structure broken
```
The node branch has the same flaw. Valid compact CSSON is silently corrupted.
**Fix:** delete exactly the declaration/rule span (use lexbor offsets for the bounds, only trimming a trailing `;`/whitespace), not the enclosing line.

### H4 — Comment-unaware hand-rolled brace/quote matcher **[confirmed]**
**Where:** the node-end scan in `csson_remove`, and `blk_close` (used by `op_add`/`op_replace`).
**Techniques:** Symbolic Execution, Protocol Conformance, Semantic-Gap, plus a **prime-directive** check.

The matcher counts `{`/`}` and tracks `'`/`"` strings but **ignores CSS comments**:
```
input : cssonv1{ a { /* } */ --x: 1; }  b { --y: 2; } }
rm /a/0 →
cssonv1{
*/ --x: 1; }
  b { --y: 2; }          # stopped at the `}` inside the comment → corruption
```
A `}`, `{`, `"` or `'` inside a comment misaligns the scan. **Additionally**, `CLAUDE.md` explicitly forbids "brace/quote/comment/paren matching to find blocks" — this hand-rolled matcher is itself a prime-directive violation and the root cause of the corruption.
**Fix:** obtain the rule's end offset from lexbor (the parser already knows it) instead of re-scanning; never hand-match CSS structure.

---

## Medium

### M1 — Object keys are not JSON-escaped **[by-inspection]**
**Where:** `node_json` writes key bytes via `memcpy` with no escaping.
**Techniques:** Information-Flow/Taint, Protocol Conformance.
Custom-property names may contain CSS escapes that lexbor resolves to characters needing JSON escaping (`"`, `\`, controls). Such a key yields invalid JSON. Same root cause as C2; the fix (a JSON escaper) must cover keys too.

### M2 — Huge integers: parity divergence + precision loss **[confirmed]**
**Where:** `coerce` integer branch.
**Techniques:** Boundary/Equivalence, Arithmetic Correctness, Regression/Parity.
`--big: 99999999999999999999999999` → `{"big":99999999999999999999999999}`. Syntactically valid JSON, but unrepresentable as a double; the browser's `parseInt` yields a rounded value (e.g. `1e+26`), so the engines diverge and downstream JSON consumers lose precision.
**Fix:** decide and pin the canonical representation for out-of-`int53`/`int64` integers (string vs number) consistently across engines.

### M3 — Empty/root pointer deletes the whole document **[confirmed]**
**Where:** `resolve` returns the root node for an empty token list; `csson_remove` then removes it.
**Techniques:** Equivalence Partitioning, Idempotency, API-Surface.
`csson rm file /` (or `""`) empties the file (removes the root `cssonv1{…}`). Likely surprising; an attacker-supplied pointer of `/` wipes content.
**Fix:** reject removal of the document root (or require an explicit flag).

### M4 — Selector/key truncated at 255 bytes **[by-inspection]**
**Where:** `sbcb` silently drops bytes once `s->n + l >= s->cap`; `ntype` uses `char t[256]`.
**Techniques:** Boundary, Adversarial Input.
A selector/type name longer than 255 bytes is silently truncated → wrong/merged node key, with no error. `nth_child`/`parse_root`/`node_json` all use the 256-byte buffer.
**Fix:** size the buffer to the serialised length (lexbor can report it) or error on overflow.

---

## Low

### L1 — JSON Pointer truncated at 64 segments **[by-inspection]**
`split_pointer` caps tokens at 64 (`char *tok[64]`); deeper pointers are silently truncated → edits resolve against the wrong target. Fix: error on overflow, or grow dynamically.

### L2 — Unchecked lexbor allocation **[by-inspection]**
`lxb_css_parser_create()` / `lxb_css_stylesheet_create()` results are used without NULL checks (`parse_root`, `csson_c.c` main). On OOM, `lxb_css_parser_init(NULL, …)` dereferences null. Fix: check and return an error.

### L3 — `csson_set(value=NULL)` empties the value **[by-inspection]**
`splice(..., ins=NULL)` treats null as empty insert, so a NULL value silently blanks the field instead of erroring. Fix: validate `value != NULL`.

### L4 — `uint64 > INT64_MAX` mis-serialised **[by-inspection]**
`ser_scalar` prints integers with `%lld` of `yyjson_get_sint`; a JSON patch value above `INT64_MAX` (a yyjson uint) is reinterpreted as negative. Fix: branch on `yyjson_is_uint` and use `%llu`.

### L5 — Unchecked growth arithmetic **[by-inspection]**
`bput`'s `o->cap = (o->n + l + 1) * 2` and the `sizeof(ent_t) * (ne + 1)` reallocs can overflow `size_t` for pathological sizes (would require ≈`SIZE_MAX` input, so practically OOM-bounded). Fix: overflow-checked growth for defence in depth.

---

## Techniques that yielded no findings (and why)

These were applied and are reported empty deliberately:

- **Concurrency / Happens-Before / Temporal liveness/deadlock (4, 7):** the library is pure and stateless — no globals, threads, locks, or shared mutable state. `libcsson` is reentrant; there is nothing to race or deadlock.
- **State-machine / lifecycle (2):** no stateful object with a lifecycle; each call parses → walks/splices → frees. No invalid-state reachability.
- **Authentication/Authorization, Sessions, SSRF/CSRF/Confused-Deputy, Privilege/Isolation, Secrets/Crypto (26, 31, 32, 30, 29):** no auth, sessions, network egress, multi-tenancy, privileges, or cryptography in this code — not applicable.
- **Upgrade/Migration (15):** single standard version (v1), no on-disk format migration.
- **Resource-leak (11):** traced every `xmalloc`/`xrealloc`/lexbor create to its free across all error paths in `csson_patch`/`op_*`/`node_json`/CLI `main` — no leaks found (the recent CLI cleanup-on-all-paths and per-op `free(buf)` hold up). `xmalloc` aborts on OOM (defined, no half-state).
- **Observability (24):** errors are surfaced via `*err`; adequate for a library (no logging/metrics expected here).

## Resolution

All findings are fixed in `core/src/csson_core.c` (read/edit core) and
`core/cli/csson.c` (CLI). Regressions are locked in by `ctest` (`canon_v1`,
`canon_numeric`, `canon_edge`, `edits`) run under ASan+UBSan, and by the
cross-engine `conformance/v1/verify.sh` (C core ↔ Chrome ↔ Firefox byte-identical).

| # | Fix |
|---|---|
| **C1** | `coerce` emits a number only for a *canonical* integer (`is_canonical_int`: `0` or `-?[1-9][0-9]*`); `007`/`-0` now serialise as JSON strings. |
| **C2** | New shared JSON escaper (`bjson`) escapes `"` `\`, the C0 short escapes (`\b\f\n\r\t`) and `\u00XX`; all string scalars go through it. |
| **H1** | `node_json` recursion is bounded by `CSSON_MAX_DEPTH` (512); over-deep input errors cleanly instead of overflowing the stack. Verified by the `edits` test (20k-deep input, no signal exit). |
| **H2** | `csson_set` validates the value with `valid_scalar` (lexbor re-parse of `x{--v:VALUE;}` requiring exactly one declaration, no child rules); injection payloads are rejected with a non-zero exit. |
| **H3** | `remove_content` is line-aware: it removes the whole line only when the span is surrounded by whitespace, else exactly the span — no corruption of line-sharing fields. |
| **H4** | `blk_open`/`blk_close` skip comments and strings (`skip_comment`/`at_comment`), so a `}` inside a comment/string no longer mis-terminates the block. |
| **M1** | Object keys are escaped through the same `bjson` escaper as values. |
| **M2** | `int_in_safe_range` keeps integers ≤ `2^53−1` as numbers; larger values stay verbatim strings (matches `Number.isSafeInteger`). |
| **M3** | Empty/root pointer is rejected (`csson_remove` refuses root removal; `split_pointer` overflow returns −1). |
| **M4** | `sel_type` builds the node type into a dynamically grown buffer — no 255-byte truncation. |
| **L1** | `split_pointer` returns −1 on > 64 segments instead of silently truncating. |
| **L2** | lexbor `create`/`init` return values are checked in `parse_root`. |
| **L3** | `csson_set(value=NULL)` is rejected (NULL check) rather than emptying the value. |
| **L4** | `ser_scalar` handles `yyjson_is_uint` (no `%lld` mis-serialisation of `uint64 > INT64_MAX`). |
| **L5** | All growth arithmetic routes through overflow-checked `bput`/`xmalloc`/`xrealloc` (abort-on-OOM). |

## Conclusion
The dominant theme is **the read path's manual JSON construction** (C1, C2, M1, M2) — it is not a real JSON serialiser, so it emits invalid JSON and diverges from the browser on several input classes — and **the edit path's hand-rolled text/brace manipulation** (H3, H4, plus the H2 injection), which corrupts structure on compact input, near comments, and on untrusted values. H1 (DoS) and the boundary/robustness items round out the list. Fixing C1/C2/M1 (one shared JSON escaper + integer normalisation) and H3/H4 (use lexbor's rule-end offsets; stop hand-matching) would resolve the majority by root cause.
