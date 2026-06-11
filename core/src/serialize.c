#include "serialize.h"
#include <stdio.h>
#include "json.h"
#include "mem.h"

bool is_safe_name(const char *s) {
    if (!s || !*s)
        return false;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '-' || c == '_' || c >= 0x80;
        if (!ok)
            return false;
    }
    return true;
}

int ser_scalar(yyjson_val *v, buf_t *o, char **err) {
    if (yyjson_is_str(v)) {
        json_escape(o, yyjson_get_str(v), yyjson_get_len(v)); /* escaped CSS/JSON string */
        return 0;
    }
    if (yyjson_is_uint(v)) {
        char t[32];
        int m = snprintf(t, sizeof t, "%llu", (unsigned long long)yyjson_get_uint(v));
        buf_put(o, t, (size_t)m);
        return 0;
    }
    if (yyjson_is_sint(v)) {
        char t[32];
        int m = snprintf(t, sizeof t, "%lld", (long long)yyjson_get_sint(v));
        buf_put(o, t, (size_t)m);
        return 0;
    }
    if (yyjson_is_bool(v)) {
        buf_str(o, yyjson_get_bool(v) ? "true" : "false");
        return 0;
    }
    if (yyjson_is_null(v)) {
        buf_str(o, "null");
        return 0;
    }
    if (yyjson_is_real(v)) {
        dup_err(err, "non-integer number is not representable in CSSON v1");
        return -1;
    }
    dup_err(err, "unsupported scalar value");
    return -1;
}

int ser_node(const char *type, yyjson_val *obj, int ind, buf_t *o, char **err) {
    if (!is_safe_name(type)) {
        dup_err(err, "unsafe node type (only [A-Za-z0-9_-] and non-ASCII allowed)");
        return -1;
    }
    buf_spaces(o, ind);
    buf_str(o, type);
    buf_str(o, " {\n");
    if (ser_body(obj, ind + 2, o, err))
        return -1;
    buf_spaces(o, ind);
    buf_str(o, "}");
    return 0;
}

int ser_body(yyjson_val *obj, int ind, buf_t *o, char **err) {
    if (!yyjson_is_obj(obj)) {
        dup_err(err, "node value must be a JSON object");
        return -1;
    }
    size_t i, max;
    yyjson_val *k, *v;
    yyjson_obj_foreach(obj, i, max, k, v) {
        const char *key = yyjson_get_str(k);
        if (!is_safe_name(key)) {
            dup_err(err, "unsafe field key (only [A-Za-z0-9_-] and non-ASCII allowed)");
            return -1;
        }
        if (yyjson_is_obj(v)) {
            if (ser_node(key, v, ind, o, err))
                return -1;
            buf_str(o, "\n");
        } else if (yyjson_is_arr(v)) {
            size_t j, jm;
            yyjson_val *el;
            yyjson_arr_foreach(v, j, jm, el) {
                if (!yyjson_is_obj(el)) {
                    dup_err(err, "array elements must be objects (no value-arrays in v1)");
                    return -1;
                }
                if (ser_node(key, el, ind, o, err))
                    return -1;
                buf_str(o, "\n");
            }
        } else {
            buf_spaces(o, ind);
            buf_str(o, "--");
            buf_str(o, key);
            buf_str(o, ": ");
            if (ser_scalar(v, o, err))
                return -1;
            buf_str(o, ";\n");
        }
    }
    return 0;
}
