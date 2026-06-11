/* version.c — small, stable library ABI: versions, string release, error kinds. */
#include <stdlib.h>
#include <string.h>
#include "csson.h"
#include "mem.h" /* C23 static_assert */

const char *csson_supported_versions(void) {
    return "1";
}

void csson_free_string(char *s) {
    free(s);
}

/* Best-effort classification of an *err message into a kind, so callers can
 * branch programmatically. Checked most-specific first; the messages are stable
 * across the library. Source positions are not yet surfaced (lexbor is a
 * forgiving parser and does not expose them) — that is future work. */
csson_error_kind csson_error_kind_of(const char *err) {
    if (!err)
        return CSSON_OK;
    if (strstr(err, "no root"))
        return CSSON_E_NO_ROOT;
    if (strstr(err, "not valid JSON") || strstr(err, "parse failed"))
        return CSSON_E_PARSE;
    if (strstr(err, "exceeds the supported depth"))
        return CSSON_E_DEPTH;
    if (strstr(err, "did not resolve") || strstr(err, "not found") ||
        strstr(err, "must address a field") || strstr(err, "path not found") ||
        strstr(err, "did not resolve to"))
        return CSSON_E_NOT_FOUND;
    if (strstr(err, "pointer") && (strstr(err, "too deep") || strstr(err, "needs")))
        return CSSON_E_POINTER;
    if (strstr(err, "scalar") || strstr(err, "unsafe") || strstr(err, "representable") ||
        strstr(err, "alter document structure") || strstr(err, "value is NULL"))
        return CSSON_E_VALUE;
    if (strstr(err, "patch") || strstr(err, "operation") || strstr(err, "test failed") ||
        strstr(err, "requires") || strstr(err, "unknown op") || strstr(err, "value-arrays"))
        return CSSON_E_PATCH;
    return CSSON_E_OTHER;
}
