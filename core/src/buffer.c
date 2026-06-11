#include "buffer.h"
#include <stdlib.h>
#include <string.h>
#include "mem.h"

void buf_put(buf_t *o, const char *s, size_t len) {
    if (o->n + len + 1 < o->n)
        abort(); /* size_t overflow */
    if (o->n + len + 1 > o->cap) {
        size_t need = o->n + len + 1;
        o->cap = (need > o->cap * 2) ? need : o->cap * 2;
        o->b = xrealloc(o->b, o->cap);
    }
    memcpy(o->b + o->n, s, len);
    o->n += len;
    o->b[o->n] = 0;
}

void buf_str(buf_t *o, const char *s) {
    buf_put(o, s, strlen(s));
}

void buf_spaces(buf_t *o, int n) {
    while (n-- > 0)
        buf_put(o, " ", 1);
}
