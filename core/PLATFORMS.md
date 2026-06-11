# Platforms & architectures

CSSON ships for **amd64 (x86-64) and ARM64** as first-class targets. The core is
portable C over lexbor, so it builds with the system toolchain on each platform.
CPU architecture never affects semantics; `libcsson` serves every CSSON standard
version it was built with.

## Initial release matrix (core set; edge targets deferred)
| OS | x86-64 | ARM64 | Toolchain |
|---|---|---|---|
| Linux (glibc) | ✓ | ✓ | gcc/clang |
| macOS | ✓ | ✓ | clang; ship a **universal2** dylib (`lipo`) |
| Windows | ✓ | ✓ | clang/MSVC |
| Web | wasm32 (arch-neutral) | — | Emscripten (lexbor + core → `.wasm`) |

Deferred until there's demand: Linux **musl** (Alpine/static), `wasm32-wasi`.

## Artifacts
```
libcsson.a / .so / .dylib / .dll      # the C ABI (core/include/csson.h), statically links lexbor
csson[.exe]                           # the CLI
csson.wasm                            # lexbor + core compiled to WASM
```
lexbor is built size-trimmed (`core` + `css` + `posix` port, `-Os`) and linked
statically, so consumers need only `libcsson` — no separate lexbor dependency.

## Per-binding multi-arch distribution
Each ecosystem ships the right `libcsson` for the host arch its native way:
- **Python** — `cibuildwheel`: manylinux + musllinux (x86_64/aarch64), macOS
  universal2, Windows amd64/arm64; `cffi`/`ctypes` over the bundled `libcsson`.
- **Node/npm** — platform packages via `optionalDependencies` (esbuild model); a
  prebuilt N-API addon or the WASM build.
- **.NET** — `runtime.<rid>` packages (P/Invoke).
- **Java/JVM** — arch-classified jars; load `libcsson` from resources by `os.arch`.
- **Go** — cgo per `GOOS`/`GOARCH`, with a pure-Go fallback shelling to the CLI.
- **Homebrew/apt/scoop** — prebuilt `csson` CLI per arch.

## Building
```
core/build.sh                         # clones + trims lexbor, builds libcsson + csson
# cross-compile example (Linux -> arm64):
CC=aarch64-linux-gnu-gcc core/build.sh
```
A CI release matrix produces every cell above and publishes the named artifacts.
