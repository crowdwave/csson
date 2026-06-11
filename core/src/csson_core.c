/* csson_core.c — CSSON reference core on liblexbor.
 *
 * Read  : parse with lexbor, walk the authored rule tree, emit canonical JSON.
 * Edit  : parse with lexbor, splice the original source bytes at lexbor-provided
 *         offsets (value_begin/value_end, prelude_begin/end) — comments preserved.
 * No hand-written CSS parsing anywhere (see ../../CLAUDE.md). The closing-brace
 * scan in the edit path is delimiter location for splicing (lexbor exposes no
 * block-end offset), made comment/string-aware so it cannot be fooled.
 */
// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp): feature-test macro
#define _POSIX_C_SOURCE 200809L /* strdup */
#include <ctype.h>
#include <errno.h>
#include <lexbor/css/css.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/csson.h"
#include "../third_party/yyjson.h" /* JSON parser for the RFC 6902 patch document */

static_assert(__STDC_VERSION__ >= 202311L, "CSSON requires C23 (compile with -std=c23)");

/* Max node nesting the reader will walk; deeper input is rejected, not crashed. */
#define CSSON_MAX_DEPTH 512
/* Integers with |v| <= 2^53-1 (= 9007199254740991, the JS-safe range) are emitted
   as JSON numbers; larger ones become strings — see int_in_safe_range(). */

/* Allocation wrappers: abort on OOM — defined behaviour, no leaks or null derefs. */
static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p)
        abort();
    return p;
}
static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q)
        abort();
    return q;
}
static char *xstrdup(const char *s) {
    char *d = strdup(s);
    if (!d)
        abort();
    return d;
}

/* ------------------------------------------------------ growable buffer --- */
typedef struct {
    char *b;
    size_t n, cap;
} buf_t;

static void bput(buf_t *o, const char *s, size_t l) {
    if (o->n + l + 1 < o->n)
        abort(); /* size_t overflow */
    if (o->n + l + 1 > o->cap) {
        size_t need = o->n + l + 1;
        o->cap = (need > o->cap * 2) ? need : o->cap * 2;
        o->b = xrealloc(o->b, o->cap);
    }
    memcpy(o->b + o->n, s, l);
    o->n += l;
    o->b[o->n] = 0;
}
static void bs(buf_t *o, const char *s) {
    bput(o, s, strlen(s));
}
static void bsp(buf_t *o, int n) {
    while (n-- > 0)
        bput(o, " ", 1);
}

/* Append `s` (n bytes) as a properly escaped JSON string (with surrounding quotes). */
static void bjson(buf_t *o, const char *s, size_t n) {
    bput(o, "\"", 1);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '"':
            bput(o, "\\\"", 2);
            break;
        case '\\':
            bput(o, "\\\\", 2);
            break;
        case '\b':
            bput(o, "\\b", 2);
            break;
        case '\f':
            bput(o, "\\f", 2);
            break;
        case '\n':
            bput(o, "\\n", 2);
            break;
        case '\r':
            bput(o, "\\r", 2);
            break;
        case '\t':
            bput(o, "\\t", 2);
            break;
        default:
            if (c < 0x20) {
                char u[8];
                int m = snprintf(u, sizeof u, "\\u%04x", c);
                bput(o, u, (size_t)m);
            } else {
                bput(o, &s[i], 1);
            }
        }
    }
    bput(o, "\"", 1);
}
static char *json_string(const char *s, size_t n) {
    buf_t o = {0};
    bjson(&o, s, n);
    return o.b;
}

/* Parse a JSON-Pointer array index; -1 if not a plain non-negative integer. */
static int to_index(const char *s) {
    char *end;
    long v = strtol(s, &end, 10);
    return (end != s && *end == 0 && v >= 0 && v <= 2147483647L) ? (int)v : -1;
}

/* --------------------------------------------------- selector -> type ----- */
static lxb_status_t sbcb(const lxb_char_t *d, size_t l, void *c) {
    bput((buf_t *)c, (const char *)d, l);
    return LXB_STATUS_OK;
}

/* Serialise a selector to its node-type name (leading `&` and spaces stripped).
   Returns an owned string (grows as needed — no truncation). */
static char *sel_type(lxb_css_selector_list_t *sel) {
    buf_t o = {0};
    if (sel)
        lxb_css_selector_serialize_list(sel, sbcb, &o);
    if (!o.b)
        return xstrdup("");
    char *p = o.b;
    while (*p == '&' || *p == ' ')
        p++;
    if (p != o.b)
        memmove(o.b, p, strlen(p) + 1);
    return o.b;
}

static char *dup_err(char **err, const char *m) {
    if (err)
        *err = xstrdup(m);
    return nullptr;
}

static lxb_css_rule_style_t *parse_root(const char *src, size_t len, lxb_css_parser_t **par,
                                        lxb_css_stylesheet_t **sst, char **err) {
    *par = lxb_css_parser_create();
    if (!*par)
        return (void *)dup_err(err, "parser create failed");
    lxb_css_parser_init(*par, nullptr);
    *sst = lxb_css_stylesheet_create(nullptr);
    if (!*sst)
        return (void *)dup_err(err, "stylesheet create failed");
    if (lxb_css_stylesheet_parse(*sst, *par, (const lxb_char_t *)src, len) != LXB_STATUS_OK)
        return (void *)dup_err(err, "parse failed");
    for (lxb_css_rule_t *r = ((lxb_css_rule_list_t *)(*sst)->root)->first; r; r = r->next)
        if (r->type == LXB_CSS_RULE_STYLE) {
            char *t = sel_type(((lxb_css_rule_style_t *)r)->selector);
            bool is_root = strcmp(t, "cssonv1") == 0;
            free(t);
            if (is_root)
                return (lxb_css_rule_style_t *)r;
        }
    return (void *)dup_err(err, "no root `cssonv1` rule");
}

static void parse_free(lxb_css_parser_t *par, lxb_css_stylesheet_t *sst) {
    if (sst)
        lxb_css_stylesheet_destroy(sst, true);
    if (par)
        lxb_css_parser_destroy(par, true);
}

/* --------------------------------------------------------- read path ------ */
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
    return memcmp(d + st, "9007199254740991", 16) <= 0; /* equal-length, no leading zero */
}

/* Apply CSS Syntax Level 3 §3.3 input preprocessing to a verbatim byte slice so
   the value matches what a browser's CSSOM exposes (the byte-parity invariant):
   normalise CR / CRLF / FF to LF, and map NUL and ill-formed UTF-8 to U+FFFD via
   the WHATWG "maximal subpart" replacement. Digits are untouched, so numeric
   tokens (1e3, full-precision floats) are preserved. Returns an owned buffer. */
static char *preprocess(const char *s, size_t n, size_t *outlen) {
    static const char RC[3] = {(char)0xEF, (char)0xBF, (char)0xBD}; /* U+FFFD */
    buf_t o = {0};
    size_t i = 0;
    while (i < n) {
        unsigned char b = (unsigned char)s[i];
        if (b < 0x80) {
            if (b == '\r') {
                bput(&o, "\n", 1);
                if (i + 1 < n && s[i + 1] == '\n')
                    i++;
                i++;
            } else if (b == '\f') {
                bput(&o, "\n", 1);
                i++;
            } else if (b == 0x00) {
                bput(&o, RC, 3);
                i++;
            } else {
                bput(&o, &s[i], 1);
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
            bput(&o, RC, 3); /* invalid lead byte */
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
            bput(&o, RC, 3); /* maximal subpart: one U+FFFD, reprocess from next byte */
            i++;
            continue;
        }
        bput(&o, &s[i], (size_t)need + 1);
        i += (size_t)need + 1;
    }
    *outlen = o.n;
    return o.b ? o.b : xstrdup("");
}

/* Coerce a verbatim value token to an owned JSON value string (spec §5). */
static char *coerce(const char *raw, size_t rawn) {
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

typedef struct {
    char *k, *v;
} ent_t;

static int ecmp(const void *a, const void *b) {
    return strcmp(((const ent_t *)a)->k, ((const ent_t *)b)->k);
}

static char *node_json(const char *src, lxb_css_rule_style_t *style, int depth, bool *deep) {
    if (depth > CSSON_MAX_DEPTH) {
        *deep = true;
        return xstrdup("{}"); /* cheap unwind; caller discards on *deep */
    }
    ent_t *e = nullptr;
    size_t ne = 0;
    if (style->declarations)
        for (lxb_css_rule_t *r = style->declarations->first; r; r = r->next)
            if (r->type == LXB_CSS_RULE_DECLARATION) {
                lxb_css_rule_declaration_t *dc = (lxb_css_rule_declaration_t *)r;
                if (dc->type == LXB_CSS_PROPERTY__CUSTOM && dc->u.custom) {
                    const char *k = (const char *)dc->u.custom->name.data;
                    size_t kl = dc->u.custom->name.length;
                    if (kl > 2 && k[0] == '-' && k[1] == '-') {
                        k += 2;
                        kl -= 2;
                    }
                    char *key = xmalloc(kl + 1);
                    memcpy(key, k, kl);
                    key[kl] = 0;
                    /* VERBATIM source token (offsets), not lexbor's re-serialised CSSOM value,
                       for byte-parity with a browser cssRules walk (e.g. `1e3` stays `1e3`). */
                    size_t vb = dc->offset.value_begin, ve = dc->offset.value_end;
                    char *val = (ve > vb) ? coerce(src + vb, ve - vb)
                                          : coerce((const char *)dc->u.custom->value.data,
                                                   dc->u.custom->value.length);
                    /* CSS cascade: a repeated custom property is last-declaration-wins
                       (the browser collapses it), so overwrite rather than duplicate the
                       key — keeps the output valid JSON and byte-equal to a cssRules walk. */
                    size_t ex;
                    for (ex = 0; ex < ne; ex++)
                        if (strcmp(e[ex].k, key) == 0)
                            break;
                    if (ex < ne) {
                        free(e[ex].v);
                        e[ex].v = val;
                        free(key);
                    } else {
                        e = xrealloc(e, sizeof(ent_t) * (ne + 1));
                        e[ne].k = key;
                        e[ne].v = val;
                        ne++;
                    }
                }
            }
    /* Child rules group by type into JSON arrays. Each array is accumulated in a
       buf_t (append-only, no rescan) so N same-type siblings cost O(N), not O(N^2). */
    char **ty = nullptr;
    buf_t *ar = nullptr;
    size_t nt = 0;
    if (style->child)
        for (lxb_css_rule_t *c = style->child->first; c; c = c->next)
            if (c->type == LXB_CSS_RULE_STYLE) {
                lxb_css_rule_style_t *cs = (lxb_css_rule_style_t *)c;
                char *t = sel_type(cs->selector);
                char *cj = node_json(src, cs, depth + 1, deep);
                size_t gi;
                for (gi = 0; gi < nt; gi++)
                    if (!strcmp(ty[gi], t))
                        break;
                if (gi == nt) {
                    ty = xrealloc(ty, sizeof(char *) * (nt + 1));
                    ar = xrealloc(ar, sizeof(buf_t) * (nt + 1));
                    ty[nt] = xstrdup(t);
                    ar[nt] = (buf_t){0};
                    bput(&ar[nt], "[", 1);
                    nt++;
                }
                free(t);
                if (ar[gi].b[ar[gi].n - 1] != '[')
                    bput(&ar[gi], ",", 1);
                bput(&ar[gi], cj, strlen(cj));
                free(cj);
            }
    for (size_t gi = 0; gi < nt; gi++) {
        bput(&ar[gi], "]", 1);
        e = xrealloc(e, sizeof(ent_t) * (ne + 1));
        e[ne].k = ty[gi];
        e[ne].v = ar[gi].b;
        ne++;
    }
    free(ty);
    free(ar);
    if (ne)
        qsort(e, ne, sizeof(ent_t), ecmp);
    buf_t o = {0};
    bput(&o, "{", 1);
    for (size_t i = 0; i < ne; i++) {
        if (i)
            bput(&o, ",", 1);
        bjson(&o, e[i].k, strlen(e[i].k)); /* keys are JSON-escaped too */
        bput(&o, ":", 1);
        bs(&o, e[i].v);
        free(e[i].k);
        free(e[i].v);
    }
    bput(&o, "}", 1);
    free(e);
    return o.b;
}

char *csson_to_canonical_json(const char *src, size_t len, char **err) {
    lxb_css_parser_t *par = nullptr;
    lxb_css_stylesheet_t *sst = nullptr;
    lxb_css_rule_style_t *root = parse_root(src, len, &par, &sst, err);
    char *out = nullptr;
    if (root) {
        bool deep = false;
        out = node_json(src, root, 0, &deep);
        if (deep) {
            free(out);
            out = dup_err(err, "input nesting exceeds the supported depth");
        }
    }
    parse_free(par, sst);
    return out;
}

/* --------------------------------------------------------- edit path ------ */
static lxb_css_rule_declaration_t *find_field(lxb_css_rule_style_t *st, const char *key) {
    if (!st->declarations)
        return nullptr;
    for (lxb_css_rule_t *r = st->declarations->first; r; r = r->next) {
        if (r->type != LXB_CSS_RULE_DECLARATION)
            continue;
        lxb_css_rule_declaration_t *dc = (lxb_css_rule_declaration_t *)r;
        if (dc->type != LXB_CSS_PROPERTY__CUSTOM || !dc->u.custom)
            continue;
        const char *nm = (const char *)dc->u.custom->name.data;
        size_t nl = dc->u.custom->name.length;
        if (nl > 2 && nm[0] == '-' && nm[1] == '-') {
            nm += 2;
            nl -= 2;
        }
        if (strlen(key) == nl && memcmp(nm, key, nl) == 0)
            return dc;
    }
    return nullptr;
}

static lxb_css_rule_style_t *nth_child(lxb_css_rule_style_t *st, const char *type, int idx) {
    if (!st->child)
        return nullptr;
    int seen = 0;
    for (lxb_css_rule_t *c = st->child->first; c; c = c->next) {
        if (c->type != LXB_CSS_RULE_STYLE)
            continue;
        char *t = sel_type(((lxb_css_rule_style_t *)c)->selector);
        bool hit = strcmp(t, type) == 0 && seen++ == idx;
        free(t);
        if (hit)
            return (lxb_css_rule_style_t *)c;
    }
    return nullptr;
}

typedef struct {
    lxb_css_rule_declaration_t *field;
    lxb_css_rule_style_t *node;
} target_t;

static target_t resolve(lxb_css_rule_style_t *st, char **tok, int n, int i) {
    target_t t = {nullptr, nullptr};
    if (i >= n) {
        t.node = st;
        return t;
    }
    if (i == n - 1) {
        lxb_css_rule_declaration_t *f = find_field(st, tok[i]);
        if (f) {
            t.field = f;
            return t;
        }
    }
    if (i + 1 >= n)
        return t;
    lxb_css_rule_style_t *child = nth_child(st, tok[i], to_index(tok[i + 1]));
    return child ? resolve(child, tok, n, i + 2) : t;
}

/* RFC 6901 reference-token unescaping, in place: "~1" -> "/" then "~0" -> "~"
   (order matters). Done after splitting on "/", so an escaped "/" survives. */
static void unescape_token(char *s) {
    char *w = s;
    for (char *r = s; *r; r++) {
        if (r[0] == '~' && r[1] == '1') {
            *w++ = '/';
            r++;
        } else if (r[0] == '~' && r[1] == '0') {
            *w++ = '~';
            r++;
        } else {
            *w++ = *r;
        }
    }
    *w = 0;
}

/* Split a JSON Pointer into tokens; returns -1 (overflow) if > max segments. */
static int split_pointer(const char *pointer, char **tok, int max, char **buf) {
    *buf = xstrdup(pointer);
    int n = 0;
    for (char *s = strtok(*buf, "/"); s; s = strtok(nullptr, "/")) {
        if (n >= max)
            return -1;
        unescape_token(s);
        tok[n++] = s;
    }
    return n;
}

static char *splice(const char *src, size_t len, size_t a, size_t b, const char *ins) {
    size_t il = ins ? strlen(ins) : 0;
    char *out = xmalloc(a + il + (len - b) + 1);
    memcpy(out, src, a);
    if (il)
        memcpy(out + a, ins, il);
    memcpy(out + a + il, src + b, len - b);
    out[a + il + (len - b)] = 0;
    return out;
}

static size_t line_start(const char *s, size_t a) {
    while (a > 0 && s[a - 1] != '\n')
        a--;
    return a;
}
static int indent_at(const char *s, size_t a) {
    size_t ls = line_start(s, a), k = ls;
    while (k < a && s[k] == ' ')
        k++;
    return (int)(k - ls);
}
/* Skip a C-style comment beginning at k (s[k]=='/', s[k+1]=='*'); returns the
   index of the closing slash, so the caller's for-loop k++ lands just past it. */
static size_t skip_comment(const char *s, size_t len, size_t k) {
    k += 2;
    while (k + 1 < len && !(s[k] == '*' && s[k + 1] == '/'))
        k++;
    return k + 1; /* the closing slash; caller's k++ moves past it */
}
static bool at_comment(const char *s, size_t len, size_t k) {
    return s[k] == '/' && k + 1 < len && s[k + 1] == '*';
}
static size_t blk_open(const char *s, size_t len, size_t from) {
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
/* Index of the `}` that closes the block opened at `open` — comment- and
   string-aware so braces/quotes inside comments or strings can't fool it. */
static size_t blk_close(const char *s, size_t len, size_t open) {
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

/* Validate that `value` is exactly one CSSON scalar (no structural breakout):
   parse a probe with the real parser and require one declaration, no children,
   and the parsed value to match. Prevents injection via csson_set. */
static bool valid_scalar(const char *value) {
    buf_t probe = {0};
    bs(&probe, "x{--v:");
    bs(&probe, value);
    bs(&probe, ";}");
    lxb_css_parser_t *par = lxb_css_parser_create();
    bool ok = false;
    if (par) {
        lxb_css_parser_init(par, nullptr);
        lxb_css_stylesheet_t *sst = lxb_css_stylesheet_create(nullptr);
        if (sst && lxb_css_stylesheet_parse(sst, par, (const lxb_char_t *)probe.b, probe.n) ==
                       LXB_STATUS_OK) {
            lxb_css_rule_t *first = ((lxb_css_rule_list_t *)sst->root)->first;
            if (first && first->type == LXB_CSS_RULE_STYLE && !first->next) {
                lxb_css_rule_style_t *st = (lxb_css_rule_style_t *)first;
                int ndecl = 0, nchild = 0;
                if (st->declarations)
                    for (lxb_css_rule_t *r = st->declarations->first; r; r = r->next)
                        if (r->type == LXB_CSS_RULE_DECLARATION)
                            ndecl++;
                if (st->child)
                    for (lxb_css_rule_t *c = st->child->first; c; c = c->next)
                        if (c->type == LXB_CSS_RULE_STYLE)
                            nchild++;
                ok = (ndecl == 1 && nchild == 0);
            }
        }
        if (sst)
            lxb_css_stylesheet_destroy(sst, true);
        lxb_css_parser_destroy(par, true);
    }
    free(probe.b);
    return ok;
}

/* Re-read the coerced value of the field at `pointer` from a document (owned
   result, or nullptr if the pointer no longer addresses a field). Used to verify
   an edit round-trips. */
static char *read_field_coerced(const char *src, size_t len, const char *pointer) {
    lxb_css_parser_t *par = nullptr;
    lxb_css_stylesheet_t *sst = nullptr;
    lxb_css_rule_style_t *root = parse_root(src, len, &par, &sst, nullptr);
    char *res = nullptr;
    if (root) {
        char *tok[64], *buf;
        int n = split_pointer(pointer, tok, 64, &buf);
        if (n >= 0) {
            target_t tg = resolve(root, tok, n, 0);
            if (tg.field) {
                size_t vb = tg.field->offset.value_begin, ve = tg.field->offset.value_end;
                res = (ve > vb) ? coerce(src + vb, ve - vb)
                                : coerce((const char *)tg.field->u.custom->value.data,
                                         tg.field->u.custom->value.length);
            }
        }
        free(buf);
    }
    parse_free(par, sst);
    return res;
}

char *csson_set(const char *src, size_t len, const char *pointer, const char *value, char **err) {
    if (!value)
        return dup_err(err, "set: value is NULL");
    if (!valid_scalar(value))
        return dup_err(err, "set: value is not a single CSSON scalar");
    lxb_css_parser_t *par = nullptr;
    lxb_css_stylesheet_t *sst = nullptr;
    lxb_css_rule_style_t *root = parse_root(src, len, &par, &sst, err);
    char *out = nullptr;
    if (root) {
        char *tok[64], *buf;
        int n = split_pointer(pointer, tok, 64, &buf);
        if (n < 0) {
            dup_err(err, "set: pointer too deep");
        } else {
            target_t tg = resolve(root, tok, n, 0);
            if (tg.field)
                out = splice(src, len, tg.field->offset.value_begin, tg.field->offset.value_end,
                             value);
            else
                dup_err(err, "set: pointer must address a field");
        }
        free(buf);
    }
    parse_free(par, sst);
    /* Round-trip guard: the spliced value must read back as exactly the intended
       scalar. Catches context-dependent breakouts a value-only probe misses — e.g.
       an unterminated CSS comment that would swallow the following declarations. */
    if (out) {
        char *want = coerce(value, strlen(value));
        char *got = read_field_coerced(out, strlen(out), pointer);
        if (!got || strcmp(got, want) != 0) {
            free(out);
            out = dup_err(err, "set: value would alter document structure (rejected)");
        }
        free(want);
        free(got);
    }
    return out;
}

/* Remove content [a0,b0): if the line outside the span is only whitespace, drop
   the whole line (incl trailing newline); otherwise drop exactly [a0,b0) so a
   declaration/node sharing its line with others is not corrupted. */
static char *remove_content(const char *src, size_t len, size_t a0, size_t b0) {
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

char *csson_remove(const char *src, size_t len, const char *pointer, char **err) {
    lxb_css_parser_t *par = nullptr;
    lxb_css_stylesheet_t *sst = nullptr;
    lxb_css_rule_style_t *root = parse_root(src, len, &par, &sst, err);
    char *out = nullptr;
    if (root) {
        char *tok[64], *buf;
        int n = split_pointer(pointer, tok, 64, &buf);
        if (n < 0) {
            dup_err(err, "remove: pointer too deep");
        } else {
            target_t tg = resolve(root, tok, n, 0);
            if (tg.field) {
                size_t a0 = tg.field->offset.name_begin;
                size_t b0 = tg.field->offset.value_end;
                while (b0 < len && (src[b0] == ' ' || src[b0] == '\t'))
                    b0++;
                if (b0 < len && src[b0] == ';')
                    b0++;
                out = remove_content(src, len, a0, b0);
            } else if (tg.node == root) {
                dup_err(err, "remove: cannot remove the document root");
            } else if (tg.node) {
                size_t open = blk_open(src, len, tg.node->prelude_end);
                size_t close = blk_close(src, len, open);
                size_t b0 = (close < len) ? close + 1 : close;
                out = remove_content(src, len, tg.node->prelude_begin, b0);
            } else {
                dup_err(err, "remove: pointer did not resolve");
            }
        }
        free(buf);
    }
    parse_free(par, sst);
    return out;
}

/* --------------------------------------------------- JSON Patch (RFC 6902) -
 * The patch document (JSON, possibly untrusted) is parsed with yyjson (a
 * stack-safe, fuzzed parser). Each operation is realised as a source-text splice
 * via the same lexbor offsets used by set/remove, so comments survive. The patch
 * is atomic: if any op fails, the working buffer is discarded and nullptr returned.
 */

/* A field key / node type is written verbatim into CSS text, so it must not be
   able to carry CSS structure. Allow only an identifier-safe set: ASCII
   alphanumerics, '-', '_', and non-ASCII (>=0x80, CSS ident code points) — this
   rejects every ASCII delimiter (braces, parens, semicolon, colon, slash, star,
   quotes, whitespace, …) used to break out of a declaration or rule.
   Prevents structural injection through the patch API's keys and node types. */
static bool is_safe_name(const char *s) {
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

/* serialize a JSON value to CSSON text (v1: scalars + nested nodes; no value-arrays) */
static int ser_scalar(yyjson_val *v, buf_t *o, char **err) {
    if (yyjson_is_str(v)) {
        size_t n = yyjson_get_len(v);
        bjson(o, yyjson_get_str(v), n); /* properly escaped CSS/JSON string */
        return 0;
    }
    if (yyjson_is_uint(v)) {
        char t[32];
        int m = snprintf(t, sizeof t, "%llu", (unsigned long long)yyjson_get_uint(v));
        bput(o, t, (size_t)m);
        return 0;
    }
    if (yyjson_is_sint(v)) {
        char t[32];
        int m = snprintf(t, sizeof t, "%lld", (long long)yyjson_get_sint(v));
        bput(o, t, (size_t)m);
        return 0;
    }
    if (yyjson_is_bool(v)) {
        bs(o, yyjson_get_bool(v) ? "true" : "false");
        return 0;
    }
    if (yyjson_is_null(v)) {
        bs(o, "null");
        return 0;
    }
    if (yyjson_is_real(v)) {
        dup_err(err, "non-integer number is not representable in CSSON v1");
        return -1;
    }
    dup_err(err, "unsupported scalar value");
    return -1;
}
static int ser_body(yyjson_val *obj, int ind, buf_t *o, char **err);
static int ser_node(const char *type, yyjson_val *obj, int ind, buf_t *o, char **err) {
    if (!is_safe_name(type)) {
        dup_err(err, "unsafe node type (only [A-Za-z0-9_-] and non-ASCII allowed)");
        return -1;
    }
    bsp(o, ind);
    bs(o, type);
    bs(o, " {\n");
    if (ser_body(obj, ind + 2, o, err))
        return -1;
    bsp(o, ind);
    bs(o, "}");
    return 0;
}
static int ser_body(yyjson_val *obj, int ind, buf_t *o, char **err) {
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
            bs(o, "\n");
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
                bs(o, "\n");
            }
        } else {
            bsp(o, ind);
            bs(o, "--");
            bs(o, key);
            bs(o, ": ");
            if (ser_scalar(v, o, err))
                return -1;
            bs(o, ";\n");
        }
    }
    return 0;
}

static char *op_add(const char *src, size_t len, char **tok, int n, yyjson_val *v, char **err) {
    lxb_css_parser_t *par = nullptr;
    lxb_css_stylesheet_t *sst = nullptr;
    lxb_css_rule_style_t *root = parse_root(src, len, &par, &sst, err);
    char *out = nullptr;
    if (root) {
        if (yyjson_is_obj(v) || yyjson_is_arr(v)) { /* node insert */
            if (n < 2) {
                dup_err(err, "add node: path needs <type>/<index>");
            } else {
                const char *type = tok[n - 2], *pos = tok[n - 1];
                lxb_css_rule_style_t *parent = resolve(root, tok, n - 2, 0).node;
                if (!parent) {
                    dup_err(err, "add: parent node not found");
                } else {
                    size_t close = blk_close(src, len, blk_open(src, len, parent->prelude_end));
                    int cind = indent_at(src, parent->prelude_begin) + 2;
                    buf_t o = {0};
                    bool bad = false;
                    if (yyjson_is_arr(v)) {
                        size_t j, jm;
                        yyjson_val *el;
                        yyjson_arr_foreach(v, j, jm, el) {
                            if (ser_node(type, el, cind, &o, err)) {
                                bad = true;
                                break;
                            }
                            bs(&o, "\n");
                        }
                    } else if (ser_node(type, v, cind, &o, err)) {
                        bad = true;
                    } else {
                        bs(&o, "\n");
                    }
                    if (!bad) {
                        /* Byte offset the new node should precede: just before the
                           parent's closing brace for append ("-"), else the nth sibling. */
                        size_t tgt;
                        if (strcmp(pos, "-") == 0) {
                            tgt = close;
                        } else {
                            lxb_css_rule_style_t *sib = nth_child(parent, type, to_index(pos));
                            tgt = sib ? sib->prelude_begin : close;
                        }
                        /* If the target's line holds only indentation (the canonical
                           multi-line layout), insert at the line start. Otherwise the
                           document is compact/single-line: inject the node on its own
                           line with a leading newline so it stays INSIDE the parent
                           instead of landing before it. */
                        size_t ls = line_start(src, tgt);
                        bool line_indent_only = true;
                        for (size_t k = ls; k < tgt; k++)
                            if (src[k] != ' ' && src[k] != '\t') {
                                line_indent_only = false;
                                break;
                            }
                        if (!o.b) {
                            out = splice(src, len, tgt, tgt, nullptr); /* nothing (empty array) */
                        } else if (line_indent_only) {
                            out = splice(src, len, ls, ls, o.b);
                        } else {
                            buf_t w = {0};
                            bput(&w, "\n", 1);
                            bput(&w, o.b, strlen(o.b));
                            out = splice(src, len, tgt, tgt, w.b);
                            free(w.b);
                        }
                    }
                    free(o.b);
                }
            }
        } else { /* scalar field */
            if (n < 1) {
                dup_err(err, "add field: empty path");
            } else if (!is_safe_name(tok[n - 1])) {
                dup_err(err, "unsafe field key (only [A-Za-z0-9_-] and non-ASCII allowed)");
            } else {
                const char *key = tok[n - 1];
                target_t full = resolve(root, tok, n, 0);
                if (full.field) { /* exists -> replace */
                    buf_t o = {0};
                    if (!ser_scalar(v, &o, err))
                        out = splice(src, len, full.field->offset.value_begin,
                                     full.field->offset.value_end, o.b);
                    free(o.b);
                } else {
                    lxb_css_rule_style_t *parent = resolve(root, tok, n - 1, 0).node;
                    if (!parent) {
                        dup_err(err, "add: parent node not found");
                    } else {
                        size_t open = blk_open(src, len, parent->prelude_end);
                        int cind = indent_at(src, parent->prelude_begin) + 2;
                        buf_t o = {0};
                        bs(&o, "\n");
                        bsp(&o, cind);
                        bs(&o, "--");
                        bs(&o, key);
                        bs(&o, ": ");
                        if (!ser_scalar(v, &o, err)) {
                            bs(&o, ";");
                            out = splice(src, len, open + 1, open + 1, o.b);
                        }
                        free(o.b);
                    }
                }
            }
        }
    }
    parse_free(par, sst);
    return out;
}

static char *op_replace(const char *src, size_t len, char **tok, int n, yyjson_val *v, char **err) {
    lxb_css_parser_t *par = nullptr;
    lxb_css_stylesheet_t *sst = nullptr;
    lxb_css_rule_style_t *root = parse_root(src, len, &par, &sst, err);
    char *out = nullptr;
    if (root) {
        target_t t = resolve(root, tok, n, 0);
        if (t.field) {
            if (yyjson_is_obj(v) || yyjson_is_arr(v)) {
                dup_err(err, "replace: cannot replace a scalar field with an object/array");
            } else {
                buf_t o = {0};
                if (!ser_scalar(v, &o, err))
                    out = splice(src, len, t.field->offset.value_begin, t.field->offset.value_end,
                                 o.b);
                free(o.b);
            }
        } else if (t.node) {
            if (!yyjson_is_obj(v)) {
                dup_err(err, "replace: a node must be replaced with an object");
            } else {
                size_t open = blk_open(src, len, t.node->prelude_end);
                size_t close = blk_close(src, len, open);
                int pind = indent_at(src, t.node->prelude_begin);
                buf_t o = {0};
                bs(&o, "\n");
                if (!ser_body(v, pind + 2, &o, err)) {
                    bsp(&o, pind);
                    out = splice(src, len, open + 1, close, o.b);
                }
                free(o.b);
            }
        } else {
            dup_err(err, "replace: path not found");
        }
    }
    parse_free(par, sst);
    return out;
}

static int op_test(const char *src, size_t len, const char *path, yyjson_val *v, char **err) {
    char *can = csson_to_canonical_json(src, len, err);
    if (!can)
        return -1;
    yyjson_doc *d = yyjson_read(can, strlen(can), 0);
    yyjson_val *got = d ? yyjson_doc_ptr_get(d, path) : nullptr;
    bool eq = got && yyjson_equals(got, v);
    if (d)
        yyjson_doc_free(d);
    free(can);
    if (!eq) {
        dup_err(err, "test failed: value at path does not match");
        return -1;
    }
    return 0;
}

char *csson_patch(const char *src, size_t len, const char *patch_json, char **err) {
    yyjson_doc *pd = yyjson_read(patch_json, strlen(patch_json), 0);
    if (!pd)
        return dup_err(err, "patch is not valid JSON");
    yyjson_val *arr = yyjson_doc_get_root(pd);
    if (!yyjson_is_arr(arr)) {
        yyjson_doc_free(pd);
        return dup_err(err, "patch must be a JSON array of operations");
    }

    char *work = xmalloc(len + 1);
    memcpy(work, src, len);
    work[len] = 0;
    size_t wlen = len;
    bool failed = false;
    size_t i, max;
    yyjson_val *op;
    yyjson_arr_foreach(arr, i, max, op) {
        const char *o = yyjson_get_str(yyjson_obj_get(op, "op"));
        const char *path = yyjson_get_str(yyjson_obj_get(op, "path"));
        yyjson_val *val = yyjson_obj_get(op, "value");
        const char *from = yyjson_get_str(yyjson_obj_get(op, "from"));
        if (!o || !path) {
            dup_err(err, "operation missing 'op' or 'path'");
            failed = true;
            break;
        }

        char *tok[64], *buf;
        int n = split_pointer(path, tok, 64, &buf);
        char *next = nullptr;
        bool change = true;

        if (n < 0) {
            dup_err(err, "patch: pointer too deep");
            failed = true;
        } else if (!strcmp(o, "add")) {
            if (val)
                next = op_add(work, wlen, tok, n, val, err);
            else
                dup_err(err, "add requires 'value'");
        } else if (!strcmp(o, "replace")) {
            if (val)
                next = op_replace(work, wlen, tok, n, val, err);
            else
                dup_err(err, "replace requires 'value'");
        } else if (!strcmp(o, "remove")) {
            next = csson_remove(work, wlen, path, err);
        } else if (!strcmp(o, "test")) {
            change = false;
            if (val) {
                if (op_test(work, wlen, path, val, err))
                    failed = true;
            } else {
                dup_err(err, "test requires 'value'");
                failed = true;
            }
        } else if (!strcmp(o, "move") || !strcmp(o, "copy")) {
            change = false;
            if (!from) {
                dup_err(err, "move/copy requires 'from'");
                failed = true;
            } else {
                char *can = csson_to_canonical_json(work, wlen, err);
                yyjson_doc *cd = can ? yyjson_read(can, strlen(can), 0) : nullptr;
                yyjson_val *fv = cd ? yyjson_doc_ptr_get(cd, from) : nullptr;
                if (!fv) {
                    dup_err(err, "move/copy: 'from' not found");
                    failed = true;
                } else {
                    char *base = work;
                    bool base_owned = false;
                    if (!strcmp(o, "move")) {
                        base = csson_remove(work, wlen, from, err);
                        base_owned = true;
                    }
                    if (!base) {
                        failed = true;
                    } else {
                        next = op_add(base, strlen(base), tok, n, fv, err);
                        change = true;
                        if (!next)
                            failed = true;
                    }
                    if (base_owned)
                        free(base);
                }
                if (cd)
                    yyjson_doc_free(cd);
                free(can);
            }
        } else {
            dup_err(err, "unknown op (use add/remove/replace/move/copy/test)");
            failed = true;
        }

        free(buf);
        if (failed)
            break;
        if (change) {
            if (!next) {
                failed = true;
                break;
            }
            free(work);
            work = next;
            wlen = strlen(work);
        }
    }

    yyjson_doc_free(pd);
    if (failed) {
        free(work);
        return nullptr;
    }
    return work;
}

const char *csson_supported_versions(void) {
    return "1";
}

void csson_free_string(char *s) {
    free(s);
}
