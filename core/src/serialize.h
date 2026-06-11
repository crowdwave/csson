/* serialize.h — JSON value -> CSSON text. The patch path's writer (the inverse of
 * the read path): a JSON object becomes nested style rules, scalars become custom
 * properties. v1 has no value-arrays, so a JSON array must hold objects. */
#pragma once
#include "buffer.h"
#include "../third_party/yyjson.h"

/* True if `s` is safe to write verbatim as a CSS identifier (a field key or node
 * type): [A-Za-z0-9_-] and non-ASCII only. Rejects every ASCII delimiter, so a
 * key/type from an untrusted patch cannot inject CSS structure. */
bool is_safe_name(const char *s);

/* Append a JSON scalar `v` as a CSSON value token. Returns 0, or -1 with *err. */
int ser_scalar(yyjson_val *v, buf_t *o, char **err);

/* Append a `type { ... }` node for JSON object `obj`, indented `ind`. */
int ser_node(const char *type, yyjson_val *obj, int ind, buf_t *o, char **err);

/* Append the body (fields and child nodes) of JSON object `obj`, indented `ind`. */
int ser_body(yyjson_val *obj, int ind, buf_t *o, char **err);
