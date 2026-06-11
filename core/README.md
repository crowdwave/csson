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
  src/csson_core.c    the implementation over lexbor -> libcsson
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
| `csson_set(src,len,pointer,value,err)` | replace a scalar at a JSON Pointer — comments preserved |
| `csson_remove(src,len,pointer,err)` | remove a field or node — comments preserved |
| `csson_patch(src,len,patch_json,err)` | apply an **RFC 6902 JSON Patch** — comment-preserving, atomic |
| `csson_supported_versions()` | e.g. `"1"` |
| `csson_free_string(s)` | release returned strings |

Edits address nodes with **RFC 6901 JSON Pointers** (`/department/0/team/0/lead`).
`csson_patch` applies an **RFC 6902 JSON Patch** (`add`/`remove`/`replace`/`move`/
`copy`/`test`): each op becomes a comment-preserving source splice, and the patch
is **atomic** — if any op fails (including a failed `test`), nothing is applied.
The patch JSON is parsed by **yyjson** (`third_party/`, MIT) — a stack-safe,
fuzzed parser; CSSON itself is still parsed only by lexbor.

## CLI
```
csson canon <file>                 parse → canonical JSON
csson check <file>                 exit 0 if valid CSSON
csson set   <file> <ptr> <value>   replace a scalar (comment-preserving)
csson rm    <file> <ptr>           remove a field or node (comment-preserving)
csson patch <file> <patch.json>    apply an RFC 6902 JSON Patch (comment-preserving, atomic)
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
ctest --test-dir build                 # canonical-JSON conformance tests
build/csson canon ../conformance/v1/csson_v1.csson   # == ../conformance/v1/expected.json
build/csson patch examples/sample_commented.csson examples/patch.json
```
Quality gate (all must pass clean): the strict `-Werror` build, `ctest` under
ASan+UBSan, `clang-tidy`, `cppcheck`, and `clang-format`. The core's canonical
output is byte-identical to the independent C and browser readers in
`../conformance/v1` (MD5 `9ae94e…`).

## Why lexbor
lexbor is a real CSS parser whose CSSOM exposes **source byte offsets on every
declaration and rule** (`value_begin/value_end`, `name_begin/end`,
`prelude_begin/end`) and can retain comments as tokens (`with_comment`). That
makes comment-preserving edits a matter of byte-splicing at parser-given offsets —
no hand-written parsing. lexbor is portable C, so the same core compiles to
native libs and to WASM. See `PLATFORMS.md`.
