/* stylesheet.h — the lexbor boundary.
 *
 * The ONLY place CSSON asks a real CSS parser for structure (the prime
 * directive): it parses source into lexbor's authored rule tree, locates the
 * required root `cssonv1` rule, serializes selectors to node-type names, and
 * navigates the tree by JSON-Pointer tokens. Everything downstream walks this
 * tree; nothing else calls lexbor's parser. */
#pragma once
#include <lexbor/css/css.h>

/* Serialise a selector to its node-type name (leading `&`/spaces stripped);
 * returns an owned string (grows as needed — never truncated). */
char *sel_type(lxb_css_selector_list_t *sel);

/* Parse `src` and return its root `cssonv1` style rule, or nullptr with *err set.
 * On success *par and *sst own the parse; release them with parse_free(). */
lxb_css_rule_style_t *parse_root(const char *src, size_t len, lxb_css_parser_t **par,
                                 lxb_css_stylesheet_t **sst, char **err);
void parse_free(lxb_css_parser_t *par, lxb_css_stylesheet_t *sst);

/* A resolved pointer target: at most one of {field, node} is non-null. */
typedef struct {
    lxb_css_rule_declaration_t *field;
    lxb_css_rule_style_t *node;
} target_t;

/* Find a custom-property declaration named `key` (without the `--`) on `st`. */
lxb_css_rule_declaration_t *find_field(lxb_css_rule_style_t *st, const char *key);

/* The `idx`-th child style rule of `st` whose node-type is `type` (0-based). */
lxb_css_rule_style_t *nth_child(lxb_css_rule_style_t *st, const char *type, int idx);

/* Resolve tokens tok[i..n) against `st` to a field or node target. */
target_t resolve(lxb_css_rule_style_t *st, char **tok, int n, int i);
