/* json.h — JSON string escaping (RFC 8259). The read path's only serializer:
 * everything else in the canonical output is structural punctuation or a verbatim
 * canonical integer. */
#pragma once
#include <stddef.h>
#include "buffer.h"

/* Append s[0..len) to `o` as a quoted, escaped JSON string. */
void json_escape(buf_t *o, const char *s, size_t len);

/* Return s[0..len) as a freshly-allocated quoted JSON string. */
char *json_string(const char *s, size_t len);
