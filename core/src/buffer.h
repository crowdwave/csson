/* buffer.h — a growable, NUL-terminated byte buffer.
 *
 * The single output primitive shared by the JSON writer, the CSSON serializer and
 * the value coercer. Zero-initialise with {0}; `.b` is owned by the caller and
 * released with free(). Appends abort() on size_t overflow. */
#pragma once
#include <stddef.h>

typedef struct {
    char *b;       /* owned, NUL-terminated (null while still empty) */
    size_t n, cap; /* length (excluding the NUL) and capacity */
} buf_t;

void buf_put(buf_t *o, const char *s, size_t len); /* append `len` bytes */
void buf_str(buf_t *o, const char *s);             /* append a C string */
void buf_spaces(buf_t *o, int n);                  /* append `n` spaces */
