/* patch.c — RFC 6902 JSON Patch over CSSON.
 *
 * The patch document (JSON, possibly untrusted) is parsed with yyjson (a
 * stack-safe, fuzzed parser). Each operation is realised as a source-text splice
 * via the same lexbor offsets used by set/remove, so comments survive. The patch
 * is atomic: if any op fails, the working buffer is discarded and nullptr returned. */
#include <string.h>
#include "../third_party/yyjson.h"
#include "buffer.h"
#include "csson.h"
#include "mem.h"
#include "pointer.h"
#include "serialize.h"
#include "stylesheet.h"
#include "textedit.h"

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
                            buf_str(&o, "\n");
                        }
                    } else if (ser_node(type, v, cind, &o, err)) {
                        bad = true;
                    } else {
                        buf_str(&o, "\n");
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
                            buf_put(&w, "\n", 1);
                            buf_put(&w, o.b, strlen(o.b));
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
                        buf_str(&o, "\n");
                        buf_spaces(&o, cind);
                        buf_str(&o, "--");
                        buf_str(&o, key);
                        buf_str(&o, ": ");
                        if (!ser_scalar(v, &o, err)) {
                            buf_str(&o, ";");
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
                buf_str(&o, "\n");
                if (!ser_body(v, pind + 2, &o, err)) {
                    buf_spaces(&o, pind);
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

        char *tok[CSSON_PTR_MAX_TOKENS], *buf;
        int n = split_pointer(path, tok, CSSON_PTR_MAX_TOKENS, &buf);
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
