# CSSON — project instructions

## GitHub: always operate as the `crowdwave` account

**Before ANY GitHub interaction** (`git push`, `gh ...`, PR/issue ops), check the
active account and ensure it is **`crowdwave`** — the session's `gh` keeps
reverting to `starqueue`, which has only READ access to `crowdwave/csson` and
fails with `403 Permission denied`.

```sh
gh auth status | grep "Active account"          # verify
gh auth switch --hostname github.com --user crowdwave   # if it is not crowdwave
```

Do this check every time; do not assume the previous switch is still active.

## Prime directive: CSSON NEVER writes its own parsing/processing

CSSON is a data format that *is* valid CSS. Every CSSON implementation MUST obtain
its structure from an existing, real CSS parser and only map that parser's output
to data. CSSON code must never parse or process CSS itself.

**This means, in every language/binding:**
- Parse with a real CSS parser and read the **authored rule tree** it produces —
  e.g. lexbor's CSSOM (C), the browser's `cssRules` (JS), or an equivalent
  first-class CSS parser for the target environment.
- CSSON's own code is limited to **walking that rule tree** (rules → objects,
  custom properties → fields) and **value coercion** of already-tokenized scalar
  values per the spec.

**Forbidden — these count as "writing our own parsing/processing":**
- Hand-written tokenizers, scanners, lexers, or character/byte walkers for CSS.
- Brace/quote/comment/paren matching to find blocks or split declarations.
- Regex-based extraction of selectors, declarations, or values from CSS text.
- Any "lite reader" that assembles structure from raw text instead of from a real
  CSS parser's rule tree.

If a target environment has no usable CSS parser, the answer is to bind one
(C ABI / WASM / the CLI) — never to hand-roll parsing.

## Browser-parity invariant (non-negotiable)

**Anything CSSON does must work precisely the same when done via browser
JavaScript/CSS.** The browser's native CSS engine (`document.styleSheets` →
`cssRules`, read off the authored rule tree) is a first-class conformance target,
not an afterthought. For any operation in any environment, a browser performing
the equivalent operation MUST produce byte-identical output.

- **Read / canonical JSON** — must be byte-identical to a browser walking
  `cssRules` (see `conformance/v1/readers/browser`). This is the core guarantee.
- **Scalar values** — coercion must be defined so every engine yields the same
  result regardless of how its parser serializes a token. Do not rely on a
  parser's re-serialized value; pin a canonical form in the spec and test it
  cross-engine (floats, exponents, escaped strings included).
- **Edit (set/remove/patch)** — the browser delivers these through lexbor
  compiled to **WASM** (the same engine → identical, comment-preserving), because
  the native CSSOM exposes no source byte offsets and editing via `cssText`
  strips comments. WASM-in-the-browser counts as "via browser JavaScript"; the
  result must equal the native core's byte-for-byte.

Every feature, before it ships, needs a browser-engine check in the conformance
harness proving parity.

### Reference core
The system is based on **liblexbor**. The reference core is C over lexbor
(`core/`, producing `libcsson` + the `csson` CLI):
- **Read** — parse with lexbor, walk the authored rule tree, emit canonical JSON.
- **Edit** — splice the original source bytes at lexbor-provided offsets
  (`value_begin/value_end`, `prelude_begin/end`), so comments/formatting survive.
  This is editing, not parsing, and is therefore compliant.

The current standard is **CSSON v1** (`spec/v1`). Every binding links `libcsson`
(C ABI in `core/include/csson.h`), or uses lexbor compiled to WASM. The
independent cross-engine conformance checks (`conformance/v1/readers/`) are the C
(lexbor) reader plus the two real browsers — Chrome (Blink) and Firefox (Gecko).

## C code standards (mandatory)

All C in this project is **C23** and is written to use the latest C23 features
wherever they make the code clearer, safer, or simpler — never older idioms when a
C23 one exists.

- **Standard:** compile with `-std=c23` (gcc ≥ 14, clang ≥ 18). No GNU-only
  extensions unless unavoidable and documented.
- **Prefer C23 features:** `nullptr`; `true`/`false`/`bool` as keywords (drop
  `<stdbool.h>`); `static_assert`/`alignas`/`alignof` keywords; standard
  attributes `[[nodiscard]]`, `[[maybe_unused]]`, `[[fallthrough]]`, `[[deprecated]]`;
  `typeof`/`auto`; `constexpr`; `enum` with a fixed underlying type; `_BitInt`
  where exact widths matter; `#embed` for embedding data. Use them when they
  improve the code.
- **Modern build system:** every C component has a real build system —
  **CMake (≥ 3.28)** is the canonical one (no ad-hoc shell `gcc` lines for the
  product build). It must build the C ABI lib, the CLI, and the WASM target,
  cross-compile for the amd64 + ARM64 matrix, and expose `install`/test targets.
- **Warnings are errors:** `-Wall -Wextra -Wpedantic -Wconversion -Wshadow
  -Wcast-qual -Werror`. Zero warnings is the bar.
- **Linters & analysis (all must pass clean):** `clang-format` (enforced style),
  `clang-tidy`, `cppcheck`, and `-fsanitize=address,undefined` in the test build.
  These run in CI and must be green; aim for the highest possible code quality.
- **Memory-safety cycle (mandatory, separate gate):** a dedicated check that
  drives every read/edit/patch path — including the error/cleanup paths — under
  **valgrind** (target: `0 bytes in use at exit, 0 errors`, so lexbor/yyjson
  objects are all freed) **and** ASan+UBSan+LSan, plus a fuzz sweep of malformed
  input. It is the `memcheck` ctest (`core/tests/memcheck.sh`) and is documented
  as quality-check cycle 5 in `core/README.md`. Run it on every change to C code.

This applies to the core, the C conformance reader, and any C binding. Existing
ad-hoc `build.sh` scripts are to be migrated to this standard.
