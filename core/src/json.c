#include "json.h"
#include <stdio.h>

void json_escape(buf_t *o, const char *s, size_t n) {
    buf_put(o, "\"", 1);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '"':
            buf_put(o, "\\\"", 2);
            break;
        case '\\':
            buf_put(o, "\\\\", 2);
            break;
        case '\b':
            buf_put(o, "\\b", 2);
            break;
        case '\f':
            buf_put(o, "\\f", 2);
            break;
        case '\n':
            buf_put(o, "\\n", 2);
            break;
        case '\r':
            buf_put(o, "\\r", 2);
            break;
        case '\t':
            buf_put(o, "\\t", 2);
            break;
        default:
            if (c < 0x20) {
                char u[8];
                int m = snprintf(u, sizeof u, "\\u%04x", c);
                buf_put(o, u, (size_t)m);
            } else {
                buf_put(o, &s[i], 1);
            }
        }
    }
    buf_put(o, "\"", 1);
}

char *json_string(const char *s, size_t n) {
    buf_t o = {0};
    json_escape(&o, s, n);
    return o.b;
}
