/* textedit.h — source-preserving edit primitives.
 *
 * Edits are byte splices on the original source at lexbor-provided offsets, so
 * comments and formatting survive. lexbor exposes no block-end offset, so the
 * closing brace is located here by a comment- and string-aware scan — this is
 * delimiter location for splicing, not parsing. */
#pragma once
#include <stddef.h>

/* Return src with [a,b) replaced by `ins` (null = delete); owned result. */
char *splice(const char *src, size_t len, size_t a, size_t b, const char *ins);

/* Offset of the start of the line containing `a`. */
size_t line_start(const char *s, size_t a);

/* Number of leading spaces on the line containing `a`. */
int indent_at(const char *s, size_t a);

/* Offset of the first `{` at/after `from`, skipping comments. */
size_t blk_open(const char *s, size_t len, size_t from);

/* Offset of the `}` that closes the block opened at `open` — comment- and
 * string-aware, so braces inside comments or strings cannot fool it. */
size_t blk_close(const char *s, size_t len, size_t open);

/* Delete [a0,b0): drops the whole line if the rest of it is blank, else exactly
 * the span, so a declaration/node sharing its line is not corrupted. */
char *remove_content(const char *src, size_t len, size_t a0, size_t b0);
