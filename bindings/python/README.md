# CSSON for Python

Read, edit, and validate [CSSON](../../README.md) — structured data that is also
valid CSS — from Python. A thin `ctypes` binding over the native `libcsson` C
library, so it uses the *same* QuickJS + PostCSS + csstree core as the CLI and
browsers: the canonical JSON is byte-identical across every CSSON engine.

- **No third-party dependencies** — `ctypes` is in the standard library.
- **In-process** — no subprocess; direct calls into `libcsson`.

## Install

You need the native shared library `libcsson.so` (`.dylib` on macOS, `csson.dll`
on Windows). Two ways:

**A. Build it from the repo** (needs CMake, a C23 compiler, Node):
```sh
cmake -S core -B core/build -DCMAKE_BUILD_TYPE=Release
cmake --build core/build --target csson_shared
```
The module auto-discovers `core/build/libcsson.so` when run from the repo.

**B. Ship the library** with your app: copy `libcsson.so` next to `csson.py`, or
point `$CSSON_LIB` at it:
```sh
export CSSON_LIB=/path/to/libcsson.so
```

Then put `csson.py` on your path (or `pip install .` from this directory).

## Usage

```python
import csson

text = """cssonv1 {
  --org: "Acme";          /* comments are preserved by edits */
  dept { --name: "Eng"; --n: 3; }
  dept { --name: "Ops"; }
}"""

csson.loads(text)                 # -> {'org': 'Acme', 'dept': [{'n': 3, 'name': 'Eng'}, ...]}
csson.get(text, "/dept/0/n")      # -> 3
csson.set(text, "/org", "Beta")   # -> new source text, comment intact
csson.remove(text, "/dept/1")     # -> new source text
csson.patch(text, [{"op": "replace", "path": "/org", "value": "Z"}])
csson.dumps({"app": {"port": 8080}})        # -> CSSON document
csson.validate("<integer>", "5")            # -> True   (CSS @property check)
csson.version()                             # -> "1"
```

Invalid input raises `csson.CssonError`.

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

## Notes on types
CSSON keeps cross-engine determinism, so coercion is intentional: a bare integer
becomes a number, a quoted string becomes a string, and **everything else
(decimals like `1.5`, units like `30s`, colors, `url(...)`) is a string** —
exact and lossless. Every named block is a JSON **array** (a single block is a
one-element list), mirroring the browser CSSOM. See the top-level README and
`samples/features/` for the full type/shape rules.

## Test
```sh
python3 test_csson.py        # or: python3 -m pytest
```
