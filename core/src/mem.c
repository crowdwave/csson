#include "mem.h"
#include <stdlib.h>
#include <string.h>

void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p)
        abort();
    return p;
}

void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q)
        abort();
    return q;
}

char *xstrdup(const char *s) {
    char *d = strdup(s);
    if (!d)
        abort();
    return d;
}

void *dup_err(char **err, const char *msg) {
    if (err)
        *err = xstrdup(msg);
    return nullptr;
}
