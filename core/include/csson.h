/* csson.h — C ABI for CSSON, implemented on liblexbor.
 *
 * CSSON never writes its own parsing (see ../../CLAUDE.md): every function here
 * parses with lexbor (a real CSS parser) and either walks the authored rule tree
 * (read) or splices the original source bytes at lexbor-provided offsets (edit),
 * so comments and formatting are preserved.
 *
 * All returned strings are heap-allocated; release them with csson_free_string.
 * Read/edit functions return NULL on failure and, if `err` is non-NULL, set *err
 * to an allocated message (also freed with csson_free_string).
 */
#ifndef CSSON_H
#define CSSON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Error kinds for programmatic handling. The message in *err stays the
 * human-readable detail; csson_error_kind_of() classifies it so callers can
 * branch (e.g. retry on E_NOT_FOUND, report E_PARSE). */
typedef enum {
    CSSON_OK = 0,
    CSSON_E_PARSE,     /* input is not parseable (CSS or JSON) */
    CSSON_E_NO_ROOT,   /* no root `cssonv1` rule */
    CSSON_E_NOT_FOUND, /* the pointer did not resolve to a field/node/value */
    CSSON_E_POINTER,   /* malformed or too-deep JSON Pointer / wrong target kind */
    CSSON_E_VALUE,     /* invalid scalar value, unsafe key/type, or not representable */
    CSSON_E_DEPTH,     /* nesting exceeds the supported depth */
    CSSON_E_PATCH,     /* patch document or operation error (incl. a failed test) */
    CSSON_E_OTHER
} csson_error_kind;

/* Classify a message returned via *err (CSSON_E_OTHER if unrecognised). */
csson_error_kind csson_error_kind_of(const char *err);

/* READ: parse `src` (len bytes) and return canonical JSON for the root `cssonv1`
 * node (sorted keys, compact, object arrays in source order). */
char *csson_to_canonical_json(const char *src, size_t len, char **err);

/* READ ONE VALUE: canonical JSON of the value at `pointer` (an RFC 6901 JSON
 * Pointer; "" selects the whole document). NULL if the pointer does not resolve. */
char *csson_get(const char *src, size_t len, const char *pointer, char **err);

/* CREATE: serialize a JSON object (`json`, len bytes) into a CSSON document — the
 * inverse of csson_to_canonical_json. The JSON root must be an object; nested
 * objects become nodes, arrays-of-objects become repeated nodes, scalars become
 * `--custom` fields. Non-integer numbers and value-arrays are not representable. */
char *csson_from_json(const char *json, size_t len, char **err);

/* EDIT (comment-preserving): all return the full edited source text.
 * `pointer` is an RFC-6901 JSON Pointer, e.g. "/department/0/team/0/lead". */

/* Replace the scalar value at `pointer`. `value` is the raw CSSON scalar to write
 * (e.g. "\"Alicia\"" or "42") — an advanced primitive; prefer csson_set_json. */
char *csson_set(const char *src, size_t len, const char *pointer, const char *value, char **err);

/* Replace the scalar at `pointer` from a JSON value (e.g. "\"Alicia\"", "42",
 * "true") — the ergonomic form of csson_set that does the CSSON quoting for you. */
char *csson_set_json(const char *src, size_t len, const char *pointer, const char *json_value,
                     char **err);

/* Remove the field or node addressed by `pointer`. */
char *csson_remove(const char *src, size_t len, const char *pointer, char **err);

/* Apply an RFC 6902 JSON Patch document (`patch_json`) to `src`. Operations
 * (add/remove/replace/move/copy/test) target RFC 6901 JSON Pointers and are
 * applied as comment-preserving source edits. Atomic: on any failed op the whole
 * patch is rejected (returns NULL) and the source is left unchanged. */
char *csson_patch(const char *src, size_t len, const char *patch_json, char **err);

/* Standard versions this build supports, e.g. "1". Borrowed static; do NOT free. */
const char *csson_supported_versions(void);

void csson_free_string(char *s);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CSSON_H */
