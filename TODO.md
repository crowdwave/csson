# CSSON — task list

Status of the project and the work ahead. `[x]` done · `[~]` partial · `[ ]` todo.
Invariants that gate every task: **never write our own CSS parsing** and
**browser parity** (see `CLAUDE.md`).

---

## Done
- [x] CSSON **v1** spec (draft) — `spec/v1/SPEC.md` (array-free model, `-csson.css`/`text/css`, verbatim-token §5)
- [x] **C core** on lexbor (`core/`) — `libcsson` + `csson` CLI: `canon`, `check`, `set`, `rm`
- [x] **RFC 6902 patch** (`csson_patch`, atomic, comment-preserving) via vendored yyjson
- [x] **Comment-preserving edits** via lexbor source offsets (proven)
- [x] **Verbatim scalar read** (source offsets) — fixes `1e3`/float parity
- [x] **Cross-engine conformance**: C · Chrome (Blink) · Firefox (Gecko) · core all byte-identical (canonical + numeric fixtures)
- [x] **Browser reader** — built-ins-only CSSOM (shared `conformance/v1/readers/browser/reader.js`)
- [x] **Firefox** installed & driven (geckodriver/WebDriver); Chrome via CDP
- [x] Verification runbook (`conformance/VERIFICATION.md`); platforms matrix (`core/PLATFORMS.md`)
- [x] `-csson.css` extension enforced across docs/fixtures

---

## Phase 1 — Core completeness (foundation for the fan-out)
- [~] **C code → C23** (see `CLAUDE.md` "C code standards"): `core/` + the C
      conformance reader on `-std=c23`, **mandated** (CMake `c_std_23` hard-check +
      `CMAKE_C_STANDARD_REQUIRED` + `static_assert(__STDC_VERSION__ >= 202311L)`);
      C23 idioms (`nullptr`, keyword `bool`); **CMake** build (lib + CLI, fetches
      lexbor); strict `-Werror` (`-Wall -Wextra -Wpedantic -Wconversion -Wshadow
      -Wcast-qual -Wstrict-prototypes`), clang-format, clang-tidy, cppcheck,
      ASan/UBSan — all green. **Remaining:** WASM target (Emscripten) + wire the
      gate into CI.
- [ ] **WASM front door** — compile lexbor + core to WASM (Emscripten); enables in-browser **editing** and Node usage. Add to conformance (WASM engine must match `expected.json`).
- [x] **Developer API completeness** — `csson_get` (read one value by pointer),
      `csson_from_json` (author CSSON from a JSON object), `csson_set_json` (set from
      a JSON value), `csson_error_kind` + `csson_error_kind_of` (programmatic error
      branching). CLI verbs `get`/`from-json`/`set-json`. Tests: `api` + extended
      `memcheck` (valgrind + ASan clean). (`src/jsonapi.c`)
- [x] **CSSOM-style handle API** — `csson_open`/`close`/`sheet_text`, `csson_root`,
      `csson_rule_count`/`rule_at`/`selector_text`, `csson_property_*`,
      `csson_get_property`(verbatim/typed), `csson_set_property`/`remove_property`,
      `csson_insert_rule`/`delete_rule`. Path-addressed handles over the live tree;
      edits via the atomic patch engine. Tests: `sheet` (api+valgrind clean).
      (`src/sheet.c`). **Remaining:** `CSSPropertyRule` read accessors
      (`@property` as data — needs the block re-parse trick).
- [ ] **`@property` schema validation** (`csson_validate` + CLI `validate`) —
      **decision pending:** lexbor canNOT match the CSS `syntax` grammar (it is a
      tokenizer; `@property` is an unknown at-rule). Full C-side validation requires
      **binding a real engine via a C ABI** — lightningcss or Servo/stylo (Rust) as
      a small static lib + thin C shim. Choose the engine, then implement; the
      browser harness (CSS.registerProperty / CSSStyleValue.parse) is the conformance oracle.
- [ ] **CLI distribution** — stdin polish; build matrix → prebuilt `csson` binaries (amd64/arm64 · linux/macOS/windows).
- [ ] **More conformance fixtures** — escaped strings, comments-in-values, deep nesting, empty nodes, duplicate keys, Unicode; regenerate expected; run cross-engine.
- [ ] **Ratify spec v1** — flip `Status: Draft` → ratified/frozen once the above edge cases are pinned.

## Phase 2 — First binding as the template
- [ ] **Python** (`bindings/python`) via cffi/ctypes over `libcsson` — read/edit/patch, wheels (cibuildwheel, manylinux+musllinux × amd64/arm64, macOS universal2, win).
- [ ] **Per-binding conformance hookup** — generic harness so each binding runs the `conformance/v1` fixtures and must match `expected.json`. Establishes the repeatable pattern.

## Phase 3 — The remaining environments (each: wrap `libcsson` or WASM, pass conformance)
- [ ] **Node.js** (`bindings/nodejs`) — over the WASM front door (no CSSOM in Node); also unlocks edit/patch server-side.
- [ ] **Rust** (`bindings/rust`) — `csson-sys` (bindgen FFI) + safe `csson` crate over `libcsson`. **Binding only, never a reimplementation** (the earlier standalone Rust impl was removed for duplicating parsing).
- [ ] Go (cgo) · [ ] Java/JVM (JNI/Panama) · [ ] C#/.NET (P/Invoke) · [ ] C/C++ (header) ·
      [ ] Ruby · [ ] PHP (FFI) · [ ] Shell/CLI

> **WASM** (`bindings/wasm`) is tracked in Phase 1 ("WASM front door") — it is the
> shared dependency for Node.js and in-browser editing, and a conformance engine.

## Phase 4 — Hardening & release
- [x] Memory-safety pass on the C core — valgrind (0 bytes in use at exit, 0 errors)
      + ASan/UBSan/LSan over all read/edit/patch paths incl. error/cleanup, and a
      12k-run fuzz. Locked in as `ctest -R memcheck` (`tests/memcheck.sh`; bare under
      the sanitizer build, or `MEMCHECK_VALGRIND=1` against a plain build).
- [x] Security review (untrusted `-csson.css` + patch JSON) — threat model + ranked
      findings in `docs/csson-security-analysis.md`; test set in
      `conformance/v1/security/`. **All S1–S9 fixed** (patch key/type/path injection,
      duplicate-key last-wins, set round-trip guard, CSS §3.3 preprocessing for
      parity/UTF-8, O(N²)→O(N) read, RFC 6901 `~0/~1`, CLI input cap; S6/S9 as docs).
      `security/run.sh` wired into ctest (`ctest -R security`), 0 open, parity holds.
- [ ] Fuzz the core (libFuzzer/AFL over `canon` + `patch`).
- [x] Edit-path polish: patch node-insert placement on single-line/compact docs
      (now inserts inside the parent; regression in `tests/edits.sh`).
- [ ] Release CI (build matrix, package per ecosystem, publish).
- [ ] Query/mutation API doc (`docs/query-and-mutation.md`); JSON Pointer/Patch reference.

---

## Known issues / decisions pending
- [ ] **In-browser editing** needs the WASM build (CSSOM exposes no source offsets) — blocked on Phase 1 WASM.
- [x] **git** — repo initialized; first commit `3e8e77a` pushed to `crowdwave/csson` `main` (58 files). `.gitignore` keeps build/`tmp/`/`node_modules`/`dist` out.
- [ ] **Root token renamed** `csson` → **`cssonv1`** (version-bearing, required root; spec §2 normative clause). Breaking vs any pre-existing `csson { }` docs.
