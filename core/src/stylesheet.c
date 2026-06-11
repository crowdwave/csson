#include "stylesheet.h"
#include <string.h>
#include "buffer.h"
#include "mem.h"
#include "pointer.h"

/* ---------------------------------------------------- selector -> type ---- */
static lxb_status_t sel_sink(const lxb_char_t *d, size_t l, void *ctx) {
    buf_put((buf_t *)ctx, (const char *)d, l);
    return LXB_STATUS_OK;
}

char *sel_type(lxb_css_selector_list_t *sel) {
    buf_t o = {0};
    if (sel)
        lxb_css_selector_serialize_list(sel, sel_sink, &o);
    if (!o.b)
        return xstrdup("");
    char *p = o.b;
    while (*p == '&' || *p == ' ')
        p++;
    if (p != o.b)
        memmove(o.b, p, strlen(p) + 1);
    return o.b;
}

/* ------------------------------------------------------------ parse ------- */
lxb_css_rule_style_t *parse_root(const char *src, size_t len, lxb_css_parser_t **par,
                                 lxb_css_stylesheet_t **sst, char **err) {
    *par = lxb_css_parser_create();
    if (!*par)
        return dup_err(err, "parser create failed");
    lxb_css_parser_init(*par, nullptr);
    *sst = lxb_css_stylesheet_create(nullptr);
    if (!*sst)
        return dup_err(err, "stylesheet create failed");
    if (lxb_css_stylesheet_parse(*sst, *par, (const lxb_char_t *)src, len) != LXB_STATUS_OK)
        return dup_err(err, "parse failed");
    for (lxb_css_rule_t *r = ((lxb_css_rule_list_t *)(*sst)->root)->first; r; r = r->next)
        if (r->type == LXB_CSS_RULE_STYLE) {
            char *t = sel_type(((lxb_css_rule_style_t *)r)->selector);
            bool is_root = strcmp(t, "cssonv1") == 0;
            free(t);
            if (is_root)
                return (lxb_css_rule_style_t *)r;
        }
    return dup_err(err, "no root `cssonv1` rule");
}

void parse_free(lxb_css_parser_t *par, lxb_css_stylesheet_t *sst) {
    if (sst)
        lxb_css_stylesheet_destroy(sst, true);
    if (par)
        lxb_css_parser_destroy(par, true);
}

/* ----------------------------------------------------- navigation -------- */
lxb_css_rule_declaration_t *find_field(lxb_css_rule_style_t *st, const char *key) {
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

lxb_css_rule_style_t *nth_child(lxb_css_rule_style_t *st, const char *type, int idx) {
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

target_t resolve(lxb_css_rule_style_t *st, char **tok, int n, int i) {
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
