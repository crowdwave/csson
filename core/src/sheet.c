/* sheet.c — the CSSOM-style handle API.
 *
 * A `csson_sheet` owns the current source and its parsed lexbor tree. A
 * `csson_rule` is a lightweight, path-addressed handle (its RFC 6901 pointer into
 * the tree) — reads resolve the path against the live tree, edits go through the
 * atomic, comment-preserving csson_patch and then re-parse in place. Handles are
 * owned by the sheet and survive edits (indices may shift, like the CSSOM). */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "csson.h"
#include "json.h"
#include "mem.h"
#include "pointer.h"
#include "scalar.h"
#include "stylesheet.h"
#include "textedit.h"

struct csson_sheet {
    char *src;
    size_t len;
    lxb_css_parser_t *par;
    lxb_css_stylesheet_t *sst;
    lxb_css_rule_style_t *root;
    csson_rule **handles; /* owned; freed on close */
    size_t nh;
};

struct csson_rule {
    csson_sheet *sheet;
    char *path; /* RFC 6901 pointer, "" = the root rule */
};

/* ----------------------------------------------------------- internals ---- */
static void reparse(csson_sheet *s) {
    parse_free(s->par, s->sst);
    s->par = nullptr;
    s->sst = nullptr;
    s->root = parse_root(s->src, s->len, &s->par, &s->sst, nullptr);
}

static csson_rule *handle(csson_sheet *s, const char *path) {
    csson_rule *h = xmalloc(sizeof *h);
    h->sheet = s;
    h->path = xstrdup(path);
    s->handles = xrealloc(s->handles, sizeof(csson_rule *) * (s->nh + 1));
    s->handles[s->nh++] = h;
    return h;
}

static lxb_css_rule_style_t *node_of(csson_rule *r) {
    char *tok[CSSON_PTR_MAX_TOKENS], *buf;
    int n = split_pointer(r->path, tok, CSSON_PTR_MAX_TOKENS, &buf);
    lxb_css_rule_style_t *node = nullptr;
    if (n >= 0)
        node = resolve(r->sheet->root, tok, n, 0).node;
    free(buf);
    return node;
}

/* Build "<rule.path>/<seg>" as an owned string. */
static char *child_path(csson_rule *r, const char *seg) {
    buf_t p = {0};
    buf_str(&p, r->path);
    buf_put(&p, "/", 1);
    buf_str(&p, seg);
    return p.b;
}

/* Apply one patch op to the sheet, committing + re-parsing on success. */
static int apply_patch(csson_sheet *s, const char *op, const char *path, const char *value,
                       char **err) {
    buf_t p = {0};
    buf_str(&p, "[{\"op\":\"");
    buf_str(&p, op);
    buf_str(&p, "\",\"path\":");
    json_escape(&p, path, strlen(path));
    if (value) {
        buf_str(&p, ",\"value\":");
        buf_str(&p, value);
    }
    buf_str(&p, "}]");
    char *out = csson_patch(s->src, s->len, p.b, err);
    free(p.b);
    if (!out)
        return -1;
    free(s->src);
    s->src = out;
    s->len = strlen(out);
    reparse(s);
    return 0;
}

static const char *nodash(const char *name) {
    return (name[0] == '-' && name[1] == '-') ? name + 2 : name;
}

/* ------------------------------------------------------------ lifecycle --- */
csson_sheet *csson_open(const char *src, size_t len, char **err) {
    lxb_css_parser_t *par = nullptr;
    lxb_css_stylesheet_t *sst = nullptr;
    lxb_css_rule_style_t *root = parse_root(src, len, &par, &sst, err);
    if (!root) {
        parse_free(par, sst);
        return nullptr;
    }
    csson_sheet *s = xmalloc(sizeof *s);
    s->src = xmalloc(len + 1);
    memcpy(s->src, src, len);
    s->src[len] = 0;
    s->len = len;
    s->par = par;
    s->sst = sst;
    s->root = root;
    s->handles = nullptr;
    s->nh = 0;
    return s;
}

char *csson_sheet_text(const csson_sheet *sheet) {
    return xstrdup(sheet->src);
}

void csson_close(csson_sheet *sheet) {
    if (!sheet)
        return;
    for (size_t i = 0; i < sheet->nh; i++) {
        free(sheet->handles[i]->path);
        free(sheet->handles[i]);
    }
    free(sheet->handles);
    parse_free(sheet->par, sheet->sst);
    free(sheet->src);
    free(sheet);
}

/* ----------------------------------------------------------- navigation --- */
csson_rule *csson_root(csson_sheet *sheet) {
    return handle(sheet, "");
}

size_t csson_rule_count(csson_rule *rule) {
    lxb_css_rule_style_t *node = node_of(rule);
    size_t k = 0;
    if (node && node->child)
        for (lxb_css_rule_t *c = node->child->first; c; c = c->next)
            if (c->type == LXB_CSS_RULE_STYLE)
                k++;
    return k;
}

csson_rule *csson_rule_at(csson_rule *rule, size_t i) {
    lxb_css_rule_style_t *node = node_of(rule);
    if (!node || !node->child)
        return nullptr;
    size_t k = 0;
    for (lxb_css_rule_t *c = node->child->first; c; c = c->next) {
        if (c->type != LXB_CSS_RULE_STYLE)
            continue;
        if (k == i) {
            char *type = sel_type(((lxb_css_rule_style_t *)c)->selector);
            int idx = 0; /* index among same-type prior siblings */
            for (lxb_css_rule_t *p = node->child->first; p && p != c; p = p->next)
                if (p->type == LXB_CSS_RULE_STYLE) {
                    char *pt = sel_type(((lxb_css_rule_style_t *)p)->selector);
                    if (strcmp(pt, type) == 0)
                        idx++;
                    free(pt);
                }
            char seg[64];
            int m = snprintf(seg, sizeof seg, "%s/%d", type, idx);
            free(type);
            (void)m;
            char *cp = child_path(rule, seg);
            csson_rule *h = handle(rule->sheet, cp);
            free(cp);
            return h;
        }
        k++;
    }
    return nullptr;
}

char *csson_selector_text(csson_rule *rule) {
    lxb_css_rule_style_t *node = node_of(rule);
    return node ? sel_type(node->selector) : nullptr;
}

/* --------------------------------------------------------- style block ---- */
size_t csson_property_count(csson_rule *rule) {
    lxb_css_rule_style_t *node = node_of(rule);
    size_t k = 0;
    if (node && node->declarations)
        for (lxb_css_rule_t *r = node->declarations->first; r; r = r->next)
            if (r->type == LXB_CSS_RULE_DECLARATION &&
                ((lxb_css_rule_declaration_t *)r)->type == LXB_CSS_PROPERTY__CUSTOM)
                k++;
    return k;
}

char *csson_property_name_at(csson_rule *rule, size_t i) {
    lxb_css_rule_style_t *node = node_of(rule);
    size_t k = 0;
    if (node && node->declarations)
        for (lxb_css_rule_t *r = node->declarations->first; r; r = r->next) {
            if (r->type != LXB_CSS_RULE_DECLARATION)
                continue;
            lxb_css_rule_declaration_t *dc = (lxb_css_rule_declaration_t *)r;
            if (dc->type != LXB_CSS_PROPERTY__CUSTOM || !dc->u.custom)
                continue;
            if (k == i) {
                size_t nl = dc->u.custom->name.length;
                char *out = xmalloc(nl + 1); /* full "--name", CSSOM-style */
                memcpy(out, dc->u.custom->name.data, nl);
                out[nl] = 0;
                return out;
            }
            k++;
        }
    return nullptr;
}

/* Owned copy of the verbatim, whitespace-trimmed value token (or NULL). */
static char *field_token(csson_rule *rule, const char *name) {
    lxb_css_rule_style_t *node = node_of(rule);
    if (!node)
        return nullptr;
    lxb_css_rule_declaration_t *f = find_field(node, nodash(name));
    if (!f)
        return nullptr;
    size_t vb = f->offset.value_begin, ve = f->offset.value_end;
    const char *d;
    size_t n;
    if (ve > vb) {
        d = rule->sheet->src + vb;
        n = ve - vb;
    } else {
        d = (const char *)f->u.custom->value.data;
        n = f->u.custom->value.length;
    }
    while (n && isspace((unsigned char)*d)) {
        d++;
        n--;
    }
    while (n && isspace((unsigned char)d[n - 1]))
        n--;
    char *out = xmalloc(n + 1);
    memcpy(out, d, n);
    out[n] = 0;
    return out;
}

char *csson_get_property(csson_rule *rule, const char *name) {
    return field_token(rule, name); /* verbatim value, like getPropertyValue */
}

char *csson_get_property_value(csson_rule *rule, const char *name) {
    char *tok = field_token(rule, name);
    if (!tok)
        return nullptr;
    char *out = coerce(tok, strlen(tok)); /* typed: coerced JSON scalar */
    free(tok);
    return out;
}

int csson_set_property(csson_rule *rule, const char *name, const char *json_value, char **err) {
    char *fp = child_path(rule, nodash(name));
    int rc = apply_patch(rule->sheet, "add", fp, json_value, err); /* add = create or replace */
    free(fp);
    return rc;
}

int csson_remove_property(csson_rule *rule, const char *name, char **err) {
    char *fp = child_path(rule, nodash(name));
    int rc = apply_patch(rule->sheet, "remove", fp, nullptr, err);
    free(fp);
    return rc;
}

/* ----------------------------------------------------------- structure ---- */
int csson_delete_rule(csson_rule *parent, size_t index, char **err) {
    csson_rule *child = csson_rule_at(parent, index);
    if (!child)
        return dup_err(err, "delete_rule: index out of range"), -1;
    int rc = apply_patch(parent->sheet, "remove", child->path, nullptr, err);
    return rc;
}

int csson_insert_rule(csson_rule *parent, const char *text, long index, char **err) {
    csson_sheet *s = parent->sheet;
    lxb_css_rule_style_t *node = node_of(parent);
    if (!node)
        return dup_err(err, "insert_rule: parent not found"), -1;
    size_t close = blk_close(s->src, s->len, blk_open(s->src, s->len, node->prelude_end));
    int cind = indent_at(s->src, node->prelude_begin) + 2;

    size_t target = line_start(s->src, close); /* default: append before `}` */
    if (index >= 0 && node->child) {
        long k = 0;
        for (lxb_css_rule_t *c = node->child->first; c; c = c->next)
            if (c->type == LXB_CSS_RULE_STYLE) {
                if (k == index) {
                    target = line_start(s->src, ((lxb_css_rule_style_t *)c)->prelude_begin);
                    break;
                }
                k++;
            }
    }
    buf_t ins = {0};
    buf_spaces(&ins, cind);
    buf_str(&ins, text);
    buf_str(&ins, "\n");
    char *out = splice(s->src, s->len, target, target, ins.b);
    free(ins.b);

    char *probe = csson_to_canonical_json(out, strlen(out), nullptr); /* validate the result */
    if (!probe) {
        free(out);
        return dup_err(err, "insert_rule: text produced invalid CSSON"), -1;
    }
    free(probe);
    free(s->src);
    s->src = out;
    s->len = strlen(out);
    reparse(s);
    return 0;
}
