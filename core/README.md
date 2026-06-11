# core — the CSSON reference core (C, on liblexbor)

The single source of truth for CSSON semantics, and the basis for the native
libraries and CLI across the target environments. **One parser, one core, many
front doors.**

CSSON never writes its own parsing (see `../CLAUDE.md`): the core parses with
**lexbor** and either walks the authored rule tree (read) or splices the original
source at lexbor-provided byte offsets (edit, comment-preserving).

## Contents
```
core/
  CMakeLists.txt      canonical build (C23-mandatory, strict, fetches lexbor)
  cmake/              ctest helpers
  include/csson.h     the C ABI (read + comment-preserving edit)
  src/                 libcsson, as layered modules:
    mem,buffer,json,pointer    foundation (alloc, dynamic buffer, JSON escaping, JSON Pointer)
    stylesheet                 the lexbor boundary — parse + selector + rule-tree navigation
    scalar,textedit,serialize  value coercion · source-splice primitives · JSON→CSSON writer
    read,edit,patch,version    the public ABI (canonical read · set/remove · RFC 6902 · ABI misc)
  cli/csson.c         the `csson` command-line utility
  build.sh            thin CMake wrapper
  third_party/        vendored yyjson (MIT) — parses the RFC 6902 patch document
  examples/           sample document + sample patch.json
  PLATFORMS.md        amd64 + ARM64 release matrix
```

## C ABI (`include/csson.h`)
| Function | Purpose |
|---|---|
| `csson_to_canonical_json(src,len,err)` | read → canonical JSON (sorted keys, compact) |
| `csson_get(src,len,pointer,err)` | read **one value** (JSON) at a JSON Pointer (`""` = whole doc) |
| `csson_from_json(json,len,err)` | **author** a CSSON document from a JSON object (read inverse) |
| `csson_set(src,len,pointer,value,err)` | replace a scalar from a raw CSSON token — comments preserved |
| `csson_set_json(src,len,pointer,json_value,err)` | replace a scalar from a **JSON value** (does the quoting) |
| `csson_remove(src,len,pointer,err)` | remove a field or node — comments preserved |
| `csson_patch(src,len,patch_json,err)` | apply an **RFC 6902 JSON Patch** — comment-preserving, atomic |
| `csson_supported_versions()` | e.g. `"1"` |
| `csson_error_kind_of(err)` | classify an `*err` message → `csson_error_kind` (for branching) |
| `csson_free_string(s)` | release returned strings |

### CSSOM-style handle API
For driving CSSON the way browsers expose a stylesheet — open once, navigate a
live tree, read/write through a rule's style block — the ABI also offers a handle
API that mirrors the CSSOM:

| CSSON | ≈ CSSOM |
|---|---|
| `csson_open` / `csson_sheet_text` / `csson_close` | `new CSSStyleSheet()` + `replaceSync` / `.cssText` |
| `csson_root` · `csson_rule_count` · `csson_rule_at` · `csson_selector_text` | `.cssRules` · `.length`/`.item(i)` · `.selectorText` |
| `csson_property_count` · `csson_property_name_at` · `csson_get_property` · `csson_get_property_value` | `CSSStyleDeclaration.length`/`item` · `getPropertyValue` (verbatim / typed) |
| `csson_set_property` · `csson_remove_property` | `setProperty` (create-or-replace) · `removeProperty` |
| `csson_insert_rule` · `csson_delete_rule` | `CSSGroupingRule.insertRule` · `deleteRule` |

Handles are path-addressed and owned by the sheet (freed by `csson_close`); edits
splice the source (comments preserved) and re-parse in place, so handles stay
valid across edits (indices may shift, like the CSSOM). `to/from JSON` stay as
convenience on top.

Edits address nodes with **RFC 6901 JSON Pointers** (`/department/0/team/0/lead`).
`csson_patch` applies an **RFC 6902 JSON Patch** (`add`/`remove`/`replace`/`move`/
`copy`/`test`): each op becomes a comment-preserving source splice, and the patch
is **atomic** — if any op fails (including a failed `test`), nothing is applied.
The patch JSON is parsed by **yyjson** (`third_party/`, MIT) — a stack-safe,
fuzzed parser; CSSON itself is still parsed only by lexbor.

## CLI
```
csson canon     <file>               parse → canonical JSON
csson get       <file> <ptr>         read one value (JSON) at a JSON Pointer
csson check     <file>               exit 0 if valid CSSON
csson set       <file> <ptr> <value> replace a scalar from a raw CSSON token
csson set-json  <file> <ptr> <json>  replace a scalar from a JSON value
csson rm        <file> <ptr>         remove a field or node (comment-preserving)
csson patch     <file> <patch.json>  apply an RFC 6902 JSON Patch (comment-preserving, atomic)
csson from-json <file>               serialize a JSON object into a CSSON document
csson versions
```

## Build & verify
The build is **CMake** (canonical), **C23-mandatory** (`-std=c23`,
`CMAKE_C_STANDARD_REQUIRED ON`, plus a configure-time `c_std_23` check and a
`static_assert` in the sources) with strict warnings (`-Wall -Wextra -Wpedantic
-Wconversion -Wshadow -Wcast-qual -Wstrict-prototypes -Werror`).
```
cmake -S . -B build -G Ninja           # add -DCSSON_SANITIZE=ON for ASan+UBSan, -DCSSON_TIDY=ON for clang-tidy
cmake --build build                    # -> build/libcsson.a + build/csson
ctest --test-dir build                 # conformance + edit + security + memory-safety
build/csson canon ../conformance/v1/csson_v1.csson   # == ../conformance/v1/expected.json
build/csson patch examples/sample_commented.csson examples/patch.json
```

### Quality-check cycles
Each cycle is independent and must pass clean; together they are the release gate.

| Cycle | What it proves | How to run |
|---|---|---|
| **1. Strict build** | zero warnings under `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual -Wstrict-prototypes -Werror`, C23-mandatory | `cmake --build build` |
| **2. Static analysis** | `clang-format` (style), `clang-tidy` (`-DCSSON_TIDY=ON`), `cppcheck` all clean | `clang-format -n -Werror src/*.c src/*.h` · `cppcheck --std=c23 …` · tidy build |
| **3. Conformance** | canonical JSON byte-identical to the spec fixtures **and** cross-engine (C core ↔ Chrome ↔ Firefox) | `ctest -R canon` · `../conformance/v1/verify.sh` |
| **4. Edit & security** | comment-preserving edits and the injection/parser-differential/DoS findings (S1–S9) stay fixed | `ctest -R 'edits|security'` |
| **5. Memory safety** | no leaks (lexbor/yyjson objects freed), no OOB/UAF/UB across every read/edit/patch path incl. error/cleanup paths | `ctest -R memcheck` (bare under the sanitizer build); `MEMCHECK_VALGRIND=1 bash tests/memcheck.sh <plain-build>/csson` for the valgrind pass |

**Cycle 5 — the memory-safety cycle** (`tests/memcheck.sh`) drives ~40 invocations
covering read, set/remove and all six RFC 6902 patch ops *and their error paths*
(where leaks hide), plus a fuzz sweep of malformed input. Two complementary tools:
**valgrind** on a non-sanitized build (target: `0 bytes in use at exit, 0 errors`)
and **ASan + UBSan + LSan** on the sanitizer build (`-DCSSON_SANITIZE=ON`). Run it
on every change to the C core.

The core's canonical output is byte-identical to the independent C and browser
readers in `../conformance/v1` (MD5 `9ae94e…`).

## Why lexbor
lexbor is a real CSS parser whose CSSOM exposes **source byte offsets on every
declaration and rule** (`value_begin/value_end`, `name_begin/end`,
`prelude_begin/end`) and can retain comments as tokens (`with_comment`). That
makes comment-preserving edits a matter of byte-splicing at parser-given offsets —
no hand-written parsing. lexbor is portable C, so the same core compiles to
native libs and to WASM. See `PLATFORMS.md`.
