/* jsonapi.c — JSON-bridge convenience API.
 *
 * Thin, ergonomic wrappers over the read path and the serializer:
 *   csson_get       read a single value by JSON Pointer (canonicalize + pluck)
 *   csson_from_json author a CSSON document from a JSON object (the read inverse)
 *   csson_set_json  set a scalar from a JSON value (does the CSSON quoting) */
#include <string.h>
#include "../third_party/yyjson.h"
#include "buffer.h"
#include "csson.h"
#include "mem.h"
#include "serialize.h"

char *csson_get(const char *src, size_t len, const char *pointer, char **err) {
    char *can = csson_to_canonical_json(src, len, err);
    if (!can)
        return nullptr;
    yyjson_doc *d = yyjson_read(can, strlen(can), 0);
    free(can);
    if (!d)
        return dup_err(err, "get: internal JSON read failed");
    yyjson_val *v =
        (pointer && pointer[0]) ? yyjson_doc_ptr_get(d, pointer) : yyjson_doc_get_root(d);
    char *out = nullptr;
    if (!v) {
        dup_err(err, "get: pointer did not resolve");
    } else {
        size_t n;
        char *j = yyjson_val_write(v, 0, &n); /* minified; malloc'd (libc) */
        if (j) {
            out = xmalloc(n + 1); /* re-own under our allocator/free contract */
            memcpy(out, j, n + 1);
            free(j);
        } else {
            dup_err(err, "get: value serialize failed");
        }
    }
    yyjson_doc_free(d);
    return out;
}

char *csson_from_json(const char *json, size_t len, char **err) {
    yyjson_doc *d = yyjson_read(json, len, 0);
    if (!d)
        return dup_err(err, "from-json: input is not valid JSON");
    yyjson_val *root = yyjson_doc_get_root(d);
    char *out = nullptr;
    if (!yyjson_is_obj(root)) {
        dup_err(err, "from-json: the JSON root must be an object");
    } else {
        buf_t o = {0};
        buf_str(&o, "cssonv1 {\n");
        if (!ser_body(root, 2, &o, err)) {
            buf_str(&o, "}\n");
            out = o.b;
        } else {
            free(o.b);
        }
    }
    yyjson_doc_free(d);
    return out;
}

char *csson_set_json(const char *src, size_t len, const char *pointer, const char *json_value,
                     char **err) {
    yyjson_doc *d = yyjson_read(json_value, strlen(json_value), 0);
    if (!d)
        return dup_err(err, "set-json: value is not valid JSON");
    yyjson_val *v = yyjson_doc_get_root(d);
    char *out = nullptr;
    buf_t tok = {0};
    if (ser_scalar(v, &tok, err)) /* obj/arr/real -> not a scalar */
        ;                         /* err already set */
    else
        out = csson_set(src, len, pointer, tok.b, err); /* reuse all set guards */
    free(tok.b);
    yyjson_doc_free(d);
    return out;
}
