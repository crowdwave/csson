/* version.c — small, stable library ABI: supported versions and string release. */
#include <stdlib.h>
#include "csson.h"
#include "mem.h" /* C23 static_assert */

const char *csson_supported_versions(void) {
    return "1";
}

void csson_free_string(char *s) {
    free(s);
}
