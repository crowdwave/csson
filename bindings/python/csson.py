"""CSSON for Python — a thin ctypes binding over the native libcsson (the C ABI
in core/include/csson.h). Zero third-party dependencies: ctypes is stdlib.

CSSON is structured data that is also valid CSS. This module parses/edits it via
the same QuickJS + PostCSS + csstree core the C library and browsers use, so the
canonical JSON is byte-identical across every CSSON engine.

    import csson
    data  = csson.loads(text)              # -> dict (canonical JSON, parsed)
    value = csson.get(text, "/dept/0/name")# one value at an RFC 6901 pointer
    text2 = csson.set(text, "/port", 9090) # comment-preserving edit -> new source
    text2 = csson.remove(text, "/old")
    text2 = csson.patch(text, [ {"op":"replace","path":"/x","value":1} ])
    src   = csson.dumps({"app": {"port": 8080}})   # dict -> CSSON
    ok    = csson.validate("<integer>", "5")       # @property value check

The native library is located via, in order: $CSSON_LIB, a copy next to this
file, the in-repo build (../../core/build), then the system loader.
"""
from __future__ import annotations

import ctypes
import json
import os
import sys
from ctypes import c_char_p, c_int, c_size_t, c_void_p, byref
from ctypes.util import find_library
from typing import Any

__all__ = [
    "loads", "dumps", "get", "set", "set_raw", "remove", "patch",
    "validate", "version", "CssonError",
]


class CssonError(Exception):
    """Raised when the core rejects input (parse error, bad pointer, etc.)."""


def _libname() -> str:
    return {"darwin": "libcsson.dylib", "win32": "csson.dll"}.get(sys.platform, "libcsson.so")


def _load_library() -> ctypes.CDLL:
    here = os.path.dirname(os.path.abspath(__file__))
    name = _libname()
    candidates = [
        os.environ.get("CSSON_LIB"),
        os.path.join(here, name),                                  # shipped beside the module
        os.path.join(here, "..", "..", "core", "build", name),     # in-repo dev build
    ]
    for path in candidates:
        if path and os.path.exists(path):
            return ctypes.CDLL(os.path.abspath(path))
    found = find_library("csson")
    if found:
        return ctypes.CDLL(found)
    raise CssonError(
        f"could not locate {name}; build it (cmake --build core/build --target "
        f"csson_shared) or set $CSSON_LIB to its path"
    )


_lib = _load_library()

# --- C ABI signatures -------------------------------------------------------
_lib.csson_free_string.argtypes = [c_void_p]
_lib.csson_free_string.restype = None
_lib.csson_supported_versions.restype = c_char_p  # borrowed, do NOT free

_STR_ERR = c_void_p  # returned owned char*, and char** err, handled as raw pointers
for _name, _args in {
    "csson_to_canonical_json": [c_char_p, c_size_t, ctypes.POINTER(c_void_p)],
    "csson_get": [c_char_p, c_size_t, c_char_p, ctypes.POINTER(c_void_p)],
    "csson_from_json": [c_char_p, c_size_t, ctypes.POINTER(c_void_p)],
    "csson_set": [c_char_p, c_size_t, c_char_p, c_char_p, ctypes.POINTER(c_void_p)],
    "csson_set_json": [c_char_p, c_size_t, c_char_p, c_char_p, ctypes.POINTER(c_void_p)],
    "csson_remove": [c_char_p, c_size_t, c_char_p, ctypes.POINTER(c_void_p)],
    "csson_patch": [c_char_p, c_size_t, c_char_p, ctypes.POINTER(c_void_p)],
}.items():
    getattr(_lib, _name).argtypes = _args
    getattr(_lib, _name).restype = c_void_p

_lib.csson_validate.argtypes = [c_char_p, c_char_p, ctypes.POINTER(c_void_p)]
_lib.csson_validate.restype = c_int


def _b(s: str) -> bytes:
    return s.encode("utf-8")


def _call(fn, src: str, *args: bytes | None) -> str:
    """Invoke a (src,len,...args,err) ABI function; return the owned string or raise."""
    raw = _b(src)
    err = c_void_p()
    ret = fn(raw, len(raw), *args, byref(err))
    if ret:
        out = ctypes.string_at(ret).decode("utf-8")
        _lib.csson_free_string(ret)
        return out
    msg = ctypes.string_at(err.value).decode("utf-8", "replace") if err.value else "csson error"
    if err.value:
        _lib.csson_free_string(err.value)
    raise CssonError(msg)


# --- public API -------------------------------------------------------------

def loads(text: str) -> Any:
    """Parse a CSSON document to its canonical JSON value (a dict)."""
    return json.loads(_call(_lib.csson_to_canonical_json, text))


def get(text: str, pointer: str) -> Any:
    """Read one value at an RFC 6901 JSON Pointer (\"\" selects the whole doc)."""
    return json.loads(_call(_lib.csson_get, text, _b(pointer)))


def dumps(obj: Any) -> str:
    """Serialize a JSON-compatible object to a CSSON document (inverse of loads)."""
    return _call(_lib.csson_from_json, json.dumps(obj))


def set(text: str, pointer: str, value: Any) -> str:  # noqa: A001 (deliberate API name)
    """Replace the scalar at `pointer` from a Python value; returns the new source
    (comments preserved)."""
    return _call(_lib.csson_set_json, text, _b(pointer), _b(json.dumps(value)))


def set_raw(text: str, pointer: str, token: str) -> str:
    """Advanced: replace the scalar at `pointer` from a raw CSSON token."""
    return _call(_lib.csson_set, text, _b(pointer), _b(token))


def remove(text: str, pointer: str) -> str:
    """Remove the field or node at `pointer`; returns the new source."""
    return _call(_lib.csson_remove, text, _b(pointer))


def patch(text: str, ops: list[dict]) -> str:
    """Apply an RFC 6902 JSON Patch (atomic); returns the new source."""
    return _call(_lib.csson_patch, text, _b(json.dumps(ops)))


def validate(syntax: str, value: str) -> bool:
    """True if `value` matches a CSS @property `syntax` (e.g. \"<integer>\")."""
    err = c_void_p()
    r = _lib.csson_validate(_b(syntax), _b(value), byref(err))
    if r < 0:
        msg = ctypes.string_at(err.value).decode("utf-8", "replace") if err.value else "validate error"
        if err.value:
            _lib.csson_free_string(err.value)
        raise CssonError(msg)
    return r == 1


def version() -> str:
    """The CSSON standard version this build supports (e.g. \"1\")."""
    return _lib.csson_supported_versions().decode("utf-8")
