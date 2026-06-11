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

/* READ: parse `src` (len bytes) and return canonical JSON for the root `csson`
 * node (sorted keys, compact, object arrays in source order). */
char *csson_to_canonical_json(const char *src, size_t len, char **err);

/* EDIT (comment-preserving): all return the full edited source text.
 * `pointer` is an RFC-6901 JSON Pointer, e.g. "/department/0/team/0/lead". */

/* Replace the scalar value at `pointer`. `value` is the raw CSSON scalar to write
 * (e.g. "\"Alicia\"" or "42"). */
char *csson_set(const char *src, size_t len, const char *pointer, const char *value, char **err);

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
