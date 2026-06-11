/* mem.h — allocation policy and the error-string helper.
 *
 * All allocation in libcsson goes through these wrappers, which abort() on OOM:
 * the library treats out-of-memory as fatal and unrecoverable, which keeps every
 * other path free of null-check noise and leak-on-failure bugs. */
#pragma once
#include <stddef.h>

static_assert(__STDC_VERSION__ >= 202311L, "CSSON requires C23 (compile with -std=c23)");

void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);

/* Set *err (when non-null) to an owned copy of `msg` and return nullptr — the one
 * helper behind the uniform `char **err` convention. Returns void* so a caller can
 * return it directly as any pointer type. */
void *dup_err(char **err, const char *msg);
