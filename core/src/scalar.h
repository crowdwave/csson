/* scalar.h — CSSON scalar value coercion (spec §5). */
#pragma once
#include <stddef.h>

/* Coerce a verbatim CSS value token raw[0..rawn) into an owned JSON value string:
 * CSS Syntax §3.3 input preprocessing, then "canonical safe integer, else escaped
 * string" per the spec. The result is always valid JSON and byte-equal to a
 * browser cssRules walk of the same token. */
char *coerce(const char *raw, size_t rawn);
