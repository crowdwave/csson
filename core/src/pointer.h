/* pointer.h — RFC 6901 JSON Pointer parsing (used by the edit and patch paths). */
#pragma once

/* Maximum reference tokens in a pointer; deeper pointers are rejected. */
#define CSSON_PTR_MAX_TOKENS 64

/* Split `pointer` into reference tokens (RFC 6901, with ~1/~0 unescaped). Token
 * pointers are written into tok[] and point into *buf, an owned copy the caller
 * frees. Returns the token count, or -1 if the pointer exceeds `max` tokens. */
int split_pointer(const char *pointer, char **tok, int max, char **buf);

/* Parse a JSON Pointer array index; -1 if not a plain non-negative integer. */
int to_index(const char *s);
