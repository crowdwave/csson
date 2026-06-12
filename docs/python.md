[← all docs](../README.md)

# CSSON for Python

**A thin `ctypes` binding over the native `libcsson` C library.**

**No third-party dependencies** — `ctypes` is in the standard library. It runs **in-process** (no subprocess; direct calls into `libcsson`), and because it uses the same native core as the CLI and the browsers, its output is **byte-identical to every other CSSON engine**. Files use the `.css` extension, named `<name>-csson.css`.

## Install & configure

You need the native shared library: `libcsson.so` on Linux, `libcsson.dylib` on macOS, `csson.dll` on Windows. There are two ways to get it.

### Build it from the repo

Needs CMake, a C23 compiler, and Node:

```sh
cmake -S core -B core/build -DCMAKE_BUILD_TYPE=Release
cmake --build core/build --target csson_shared
```

The module auto-discovers `core/build/libcsson.so` when run from the repo.

### Ship the library

Copy `libcsson.so` next to `csson.py`, or point `$CSSON_LIB` at it:

```sh
export CSSON_LIB=/path/to/libcsson.so
```

Then put `csson.py` on your path, or `pip install .` from [`../bindings/python`](../bindings/python/README.md). Requires Python ≥ 3.9.

> The native library is located via, in order: `$CSSON_LIB`, a copy next to `csson.py`, the in-repo build (`core/build`), then the system loader. If none is found, importing raises `csson.CssonError`.

## Usage

Given a CSSON document (e.g. `org-csson.css`), every function takes the source text as its first argument:

```python
import csson

text = """cssonv1 {
  --org: "Acme";          /* comments are preserved by edits */
  dept { --name: "Eng"; --n: 3; }
}"""

csson.loads(text)                 # {'org': 'Acme', 'dept': [{'n': 3, 'name': 'Eng'}]}
csson.get(text, "/dept/0/n")      # 3
csson.set(text, "/org", "Beta")   # new source text, comment intact
csson.patch(text, [{"op": "replace", "path": "/org", "value": "Z"}])
csson.dumps({"app": {"port": 8080}})        # object -> CSSON document
csson.validate("<integer>", "5")           # True   (CSS @property check)
```

Pointers are RFC 6901 JSON Pointers (`""` selects the whole document); `patch` takes an RFC 6902 JSON Patch and is applied atomically. Invalid input raises `csson.CssonError`.

## API

| Function | Returns | Notes |
|---|---|---|
| `loads(text)` | `dict`/value | parse to canonical JSON |
| `get(text, pointer)` | value | RFC 6901 pointer; `""` = whole doc |
| `set(text, pointer, value)` | `str` | edit from a Python value (comment-preserving) |
| `set_raw(text, pointer, token)` | `str` | edit from a raw CSSON token (advanced) |
| `remove(text, pointer)` | `str` | delete a field/node |
| `patch(text, ops)` | `str` | RFC 6902 JSON Patch (atomic) |
| `dumps(obj)` | `str` | object → CSSON |
| `validate(syntax, value)` | `bool` | `@property` value check |
| `version()` | `str` | supported standard version |

Edit functions (`set`, `set_raw`, `remove`, `patch`) return new source text and preserve comments and formatting. Any invalid input — a parse error, a bad pointer, a failed patch — raises `csson.CssonError`.

> A bare integer becomes a JSON **number**; a quoted string becomes a **string**; **everything else** (decimals like `1.5`, units like `30s`, colors, `url(...)`) is kept as an **exact string** — lossless. Every named block is a JSON **array** (a single block is a one-element list), mirroring the browser CSSOM. See the [project README](../README.md) for the full type/shape rules.
