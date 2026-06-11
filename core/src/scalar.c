#include "scalar.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "json.h"
#include "mem.h"

static void trim(const char **d, size_t *n) {
    const char *s = *d;
    size_t k = *n;
    while (k && isspace((unsigned char)*s)) {
        s++;
        k--;
    }
    while (k && isspace((unsigned char)s[k - 1]))
        k--;
    *d = s;
    *n = k;
}

/* A canonical integer literal: "0", or -?[1-9][0-9]* (no leading zeros, no "-0"). */
static bool is_canonical_int(const char *d, size_t n) {
    if (n == 0)
        return false;
    size_t st = (d[0] == '-') ? 1 : 0;
    if (st == n)
        return false;
    for (size_t i = st; i < n; i++)
        if (!isdigit((unsigned char)d[i]))
            return false;
    if (d[st] == '0')
        return n == 1; /* only bare "0" — rejects "-0", "00", "007" */
    return true;
}

/* Is the canonical integer within the JS-safe range (|v| <= 2^53-1)? */
static bool int_in_safe_range(const char *d, size_t n) {
    size_t st = (d[0] == '-') ? 1 : 0;
    size_t digits = n - st;
    if (digits < 16)
        return true; /* <= 15 digits < 2^53-1 */
    if (digits > 16)
        return false;
    return memcmp(d + st, "9007199254740991", 16) <= 0; /* equal length, no leading zero */
}

/* Apply CSS Syntax Level 3 §3.3 input preprocessing to a verbatim byte slice so
 * the value matches what a browser's CSSOM exposes (the byte-parity invariant):
 * normalise CR / CRLF / FF to LF, and map NUL and ill-formed UTF-8 to U+FFFD via
 * the WHATWG "maximal subpart" replacement. Digits are untouched, so numeric
 * tokens (1e3, full-precision floats) are preserved. Returns an owned buffer. */
static char *preprocess(const char *s, size_t n, size_t *outlen) {
    static const char RC[3] = {(char)0xEF, (char)0xBF, (char)0xBD}; /* U+FFFD */
    buf_t o = {0};
    size_t i = 0;
    while (i < n) {
        unsigned char b = (unsigned char)s[i];
        if (b < 0x80) {
            if (b == '\r') {
                buf_put(&o, "\n", 1);
                if (i + 1 < n && s[i + 1] == '\n')
                    i++;
                i++;
            } else if (b == '\f') {
                buf_put(&o, "\n", 1);
                i++;
            } else if (b == 0x00) {
                buf_put(&o, RC, 3);
                i++;
            } else {
                buf_put(&o, &s[i], 1);
                i++;
            }
            continue;
        }
        int need;
        unsigned char lo = 0x80, hi = 0xBF;
        if (b >= 0xC2 && b <= 0xDF) {
            need = 1;
        } else if (b >= 0xE0 && b <= 0xEF) {
            need = 2;
            if (b == 0xE0)
                lo = 0xA0;
            if (b == 0xED)
                hi = 0x9F;
        } else if (b >= 0xF0 && b <= 0xF4) {
            need = 3;
            if (b == 0xF0)
                lo = 0x90;
            if (b == 0xF4)
                hi = 0x8F;
        } else {
            buf_put(&o, RC, 3); /* invalid lead byte */
            i++;
            continue;
        }
        bool good = true;
        for (int k = 1; k <= need; k++) {
            if (i + (size_t)k >= n) {
                good = false;
                break;
            }
            unsigned char cb = (unsigned char)s[i + (size_t)k];
            unsigned char clo = (k == 1) ? lo : 0x80, chi = (k == 1) ? hi : 0xBF;
            if (cb < clo || cb > chi) {
                good = false;
                break;
            }
        }
        if (!good) {
            buf_put(&o, RC, 3); /* maximal subpart: one U+FFFD, reprocess from next byte */
            i++;
            continue;
        }
        buf_put(&o, &s[i], (size_t)need + 1);
        i += (size_t)need + 1;
    }
    *outlen = o.n;
    return o.b ? o.b : xstrdup("");
}

char *coerce(const char *raw, size_t rawn) {
    size_t n;
    char *owned = preprocess(raw, rawn, &n); /* CSS §3.3, then operate on the result */
    const char *d = owned;
    trim(&d, &n);
    char *out;
    if (is_canonical_int(d, n) && int_in_safe_range(d, n)) {
        out = xmalloc(n + 1); /* canonical integer -> JSON number, verbatim */
        memcpy(out, d, n);
        out[n] = 0;
    } else {
        const char *s = d; /* string: strip surrounding quotes if a quoted CSS string */
        size_t m = n;
        if (m >= 2 && d[0] == '"' && d[m - 1] == '"') {
            s = d + 1;
            m -= 2;
        }
        out = json_string(s, m); /* JSON-escaped -> always valid JSON */
    }
    free(owned);
    return out;
}
