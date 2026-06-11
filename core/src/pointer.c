#include "pointer.h"
#include <stdlib.h>
#include <string.h>
#include "mem.h"

int to_index(const char *s) {
    char *end;
    long v = strtol(s, &end, 10);
    return (end != s && *end == 0 && v >= 0 && v <= 2147483647L) ? (int)v : -1;
}

/* RFC 6901 reference-token unescaping, in place: "~1" -> "/" then "~0" -> "~"
 * (order matters). Applied after splitting on "/", so an escaped "/" survives. */
static void unescape_token(char *s) {
    char *w = s;
    for (char *r = s; *r; r++) {
        if (r[0] == '~' && r[1] == '1') {
            *w++ = '/';
            r++;
        } else if (r[0] == '~' && r[1] == '0') {
            *w++ = '~';
            r++;
        } else {
            *w++ = *r;
        }
    }
    *w = 0;
}

int split_pointer(const char *pointer, char **tok, int max, char **buf) {
    *buf = xstrdup(pointer);
    int n = 0;
    for (char *s = strtok(*buf, "/"); s; s = strtok(nullptr, "/")) {
        if (n >= max)
            return -1;
        unescape_token(s);
        tok[n++] = s;
    }
    return n;
}
