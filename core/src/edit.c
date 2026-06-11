/* edit.c — the set/remove edit API.
 *
 * Each edit parses with lexbor, resolves the JSON Pointer to a field or node, and
 * splices the original source bytes at lexbor's offsets so comments and formatting
 * survive. `set` is guarded twice: a value-only probe (valid_scalar) and a
 * round-trip read-back, so a value can never inject structure. */
#include <string.h>
#include "buffer.h"
#include "csson.h"
#include "mem.h"
#include "pointer.h"
#include "scalar.h"
#include "stylesheet.h"
#include "textedit.h"

/* Validate that `value` is exactly one CSSON scalar (no structural breakout):
 * parse a probe with the real parser and require exactly one declaration and no
 * child rules. The first line of defence for csson_set. */
static bool valid_scalar(const char *value) {
    buf_t probe = {0};
    buf_str(&probe, "x{--v:");
    buf_str(&probe, value);
    buf_str(&probe, ";}");
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
 * result, or nullptr if the pointer no longer addresses a field). */
static char *read_field_coerced(const char *src, size_t len, const char *pointer) {
    lxb_css_parser_t *par = nullptr;
    lxb_css_stylesheet_t *sst = nullptr;
    lxb_css_rule_style_t *root = parse_root(src, len, &par, &sst, nullptr);
    char *res = nullptr;
    if (root) {
        char *tok[CSSON_PTR_MAX_TOKENS], *buf;
        int n = split_pointer(pointer, tok, CSSON_PTR_MAX_TOKENS, &buf);
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
        char *tok[CSSON_PTR_MAX_TOKENS], *buf;
        int n = split_pointer(pointer, tok, CSSON_PTR_MAX_TOKENS, &buf);
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

char *csson_remove(const char *src, size_t len, const char *pointer, char **err) {
    lxb_css_parser_t *par = nullptr;
    lxb_css_stylesheet_t *sst = nullptr;
    lxb_css_rule_style_t *root = parse_root(src, len, &par, &sst, err);
    char *out = nullptr;
    if (root) {
        char *tok[CSSON_PTR_MAX_TOKENS], *buf;
        int n = split_pointer(pointer, tok, CSSON_PTR_MAX_TOKENS, &buf);
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
