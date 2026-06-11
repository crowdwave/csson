/* read.c — the read path: walk the authored rule tree, emit canonical JSON.
 *
 * Fields come from `--custom` declarations (last-wins per the CSS cascade), child
 * rules group by node-type into source-ordered arrays, keys are sorted, output is
 * compact. The whole result is byte-identical to a browser cssRules walk. */
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "csson.h"
#include "json.h"
#include "mem.h"
#include "scalar.h"
#include "stylesheet.h"

/* Max node nesting the reader will walk; deeper input is rejected, not crashed. */
#define CSSON_MAX_DEPTH 512

typedef struct {
    char *k, *v;
} ent_t;

static int ent_cmp(const void *a, const void *b) {
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
                    buf_put(&ar[nt], "[", 1);
                    nt++;
                }
                free(t);
                if (ar[gi].b[ar[gi].n - 1] != '[')
                    buf_put(&ar[gi], ",", 1);
                buf_put(&ar[gi], cj, strlen(cj));
                free(cj);
            }
    for (size_t gi = 0; gi < nt; gi++) {
        buf_put(&ar[gi], "]", 1);
        e = xrealloc(e, sizeof(ent_t) * (ne + 1));
        e[ne].k = ty[gi];
        e[ne].v = ar[gi].b;
        ne++;
    }
    free(ty);
    free(ar);
    if (ne)
        qsort(e, ne, sizeof(ent_t), ent_cmp);
    buf_t o = {0};
    buf_put(&o, "{", 1);
    for (size_t i = 0; i < ne; i++) {
        if (i)
            buf_put(&o, ",", 1);
        json_escape(&o, e[i].k, strlen(e[i].k)); /* keys are JSON-escaped too */
        buf_put(&o, ":", 1);
        buf_str(&o, e[i].v);
        free(e[i].k);
        free(e[i].v);
    }
    buf_put(&o, "}", 1);
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
