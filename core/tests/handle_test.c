/* handle_test.c — exercise the CSSOM-style handle API (csson_open/rule/…).
 * Drives navigate + read + every edit path and checks results; exits non-zero on
 * the first failure so it works as a ctest. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/csson.h"

static int fails = 0;
#define CHECK(cond, msg)                                                                            \
    do {                                                                                           \
        if (cond) {                                                                                \
            printf("  ok   %s\n", msg);                                                            \
        } else {                                                                                   \
            printf("  FAIL %s\n", msg);                                                            \
            fails++;                                                                               \
        }                                                                                          \
    } while (0)

/* str-equal helper that tolerates NULL */
static int seq(const char *a, const char *b) { return a && b && strcmp(a, b) == 0; }

int main(void) {
    const char *doc = "cssonv1 {\n"
                      "  --org: \"Acme\"; /* keep me */\n"
                      "  dept { --name: \"Eng\"; --n: 3; }\n"
                      "  dept { --name: \"Ops\"; }\n"
                      "}\n";
    char *err = nullptr;
    csson_sheet *sh = csson_open(doc, strlen(doc), &err);
    CHECK(sh != nullptr, "open");
    if (!sh) {
        fprintf(stderr, "open failed: %s\n", err ? err : "?");
        return 1;
    }

    csson_rule *root = csson_root(sh);
    char *sel = csson_selector_text(root);
    CHECK(seq(sel, "cssonv1"), "root selector = cssonv1");
    free(sel);
    CHECK(csson_rule_count(root) == 2, "root has 2 child nodes (dept, dept)");
    CHECK(csson_property_count(root) == 1, "root has 1 field (--org)");

    char *pn = csson_property_name_at(root, 0);
    CHECK(seq(pn, "--org"), "root field[0] name = --org");
    free(pn);

    char *verbatim = csson_get_property(root, "--org");
    CHECK(seq(verbatim, "\"Acme\""), "get_property(--org) verbatim = \"Acme\"");
    free(verbatim);

    char *coerced = csson_get_property_value(root, "org"); /* name w/o -- also accepted */
    CHECK(seq(coerced, "\"Acme\""), "get_property_value(org) coerced JSON = \"Acme\"");
    free(coerced);

    CHECK(csson_get_property(root, "nope") == nullptr, "get_property(absent) = NULL");

    /* navigate to dept[0] */
    csson_rule *dept0 = csson_rule_at(root, 0);
    sel = csson_selector_text(dept0);
    CHECK(seq(sel, "dept"), "child[0] selector = dept");
    free(sel);
    CHECK(csson_property_count(dept0) == 2, "dept[0] has 2 fields");
    char *n = csson_get_property_value(dept0, "n");
    CHECK(seq(n, "3"), "dept[0].n coerced = 3 (integer)");
    free(n);

    /* set a new field on dept[0] */
    int rc = csson_set_property(dept0, "lead", "\"Alice\"", &err);
    CHECK(rc == 0, "set_property(dept0, lead)");
    char *lead = csson_get_property_value(dept0, "lead");
    CHECK(seq(lead, "\"Alice\""), "dept[0].lead now = \"Alice\"");
    free(lead);

    /* comment must survive the edit */
    char *text = csson_sheet_text(sh);
    CHECK(text && strstr(text, "/* keep me */") != nullptr, "comment preserved after edit");
    free(text);

    /* insert a new child node, then delete it */
    rc = csson_insert_rule(root, "team { --x: 1; }", -1, &err);
    CHECK(rc == 0, "insert_rule(team) appended");
    CHECK(csson_rule_count(root) == 3, "root now has 3 child nodes");
    csson_rule *team = csson_rule_at(root, 2);
    sel = csson_selector_text(team);
    CHECK(seq(sel, "team"), "child[2] selector = team");
    free(sel);
    rc = csson_delete_rule(root, 2, &err);
    CHECK(rc == 0, "delete_rule(2)");
    CHECK(csson_rule_count(root) == 2, "root back to 2 child nodes");

    /* remove the field we added */
    rc = csson_remove_property(dept0, "lead", &err);
    CHECK(rc == 0, "remove_property(dept0, lead)");
    CHECK(csson_get_property(dept0, "lead") == nullptr, "dept0.lead now absent");

    /* injection guard: an unsafe node type must be rejected */
    rc = csson_insert_rule(root, "evil } x { --z: 1;", -1, &err);
    CHECK(rc == -1, "insert_rule rejects malformed/injection text");
    csson_free_string(err);
    err = nullptr;

    csson_close(sh);
    printf("handle_test: %s\n", fails ? "FAIL" : "all ok");
    return fails ? 1 : 0;
}
