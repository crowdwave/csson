#include "textedit.h"
#include <string.h>
#include "mem.h"

char *splice(const char *src, size_t len, size_t a, size_t b, const char *ins) {
    size_t il = ins ? strlen(ins) : 0;
    char *out = xmalloc(a + il + (len - b) + 1);
    memcpy(out, src, a);
    if (il)
        memcpy(out + a, ins, il);
    memcpy(out + a + il, src + b, len - b);
    out[a + il + (len - b)] = 0;
    return out;
}

size_t line_start(const char *s, size_t a) {
    while (a > 0 && s[a - 1] != '\n')
        a--;
    return a;
}

int indent_at(const char *s, size_t a) {
    size_t ls = line_start(s, a), k = ls;
    while (k < a && s[k] == ' ')
        k++;
    return (int)(k - ls);
}

/* Skip a C-style comment beginning at k (s[k]=='/', s[k+1]=='*'); returns the
 * index of the closing slash, so a caller's for-loop k++ lands just past it. */
static size_t skip_comment(const char *s, size_t len, size_t k) {
    k += 2;
    while (k + 1 < len && !(s[k] == '*' && s[k + 1] == '/'))
        k++;
    return k + 1; /* the closing slash; caller's k++ moves past it */
}

static bool at_comment(const char *s, size_t len, size_t k) {
    return s[k] == '/' && k + 1 < len && s[k + 1] == '*';
}

size_t blk_open(const char *s, size_t len, size_t from) {
    for (size_t k = from; k < len; k++) {
        if (at_comment(s, len, k)) {
            k = skip_comment(s, len, k);
            continue;
        }
        if (s[k] == '{')
            return k;
    }
    return len;
}

size_t blk_close(const char *s, size_t len, size_t open) {
    int d = 0;
    bool instr = false;
    char q = 0;
    for (size_t k = open; k < len; k++) {
        char c = s[k];
        if (instr) {
            if (c == q && s[k - 1] != '\\')
                instr = false;
            continue;
        }
        if (at_comment(s, len, k)) {
            k = skip_comment(s, len, k);
            continue;
        }
        if (c == '"' || c == '\'') {
            instr = true;
            q = c;
            continue;
        }
        if (c == '{') {
            d++;
        } else if (c == '}') {
            if (--d == 0)
                return k;
        }
    }
    return len;
}

char *remove_content(const char *src, size_t len, size_t a0, size_t b0) {
    size_t ls = line_start(src, a0);
    bool lead_ws = true;
    for (size_t i = ls; i < a0; i++)
        if (src[i] != ' ' && src[i] != '\t')
            lead_ws = false;
    size_t le = b0;
    while (le < len && src[le] != '\n')
        le++;
    bool trail_ws = true;
    for (size_t i = b0; i < le; i++)
        if (src[i] != ' ' && src[i] != '\t')
            trail_ws = false;
    if (lead_ws && trail_ws)
        return splice(src, len, ls, (le < len) ? le + 1 : le, nullptr);
    return splice(src, len, a0, b0, nullptr);
}
