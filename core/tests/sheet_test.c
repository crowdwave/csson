/* sheet_test.c — exercises the CSSOM-style handle API (open / navigate / style /
 * structure). Run under ctest; under the sanitizer build it also leak-checks. */
#include <stdio.h>
#include <string.h>
#include "csson.h"

static int fails = 0;
#define CHECK(cond, msg)                                                                           \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            printf("FAIL: %s\n", msg);                                                             \
            fails++;                                                                               \
        }                                                                                          \
    } while (0)

static int eq(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

/* take an owned string, compare, free */
static int eqf(char *got, const char *want) {
    int r = eq(got, want);
    csson_free_string(got);
    return r;
}

int main(void) {
    const char *doc = "cssonv1 {\n"
                      "  --org: \"Acme\";\n"
                      "  dept { --name: \"Eng\"; --n: 3; }\n"
                      "  dept { --name: \"Ops\"; }\n"
                      "}\n";
    char *err = NULL;
    csson_sheet *s = csson_open(doc, strlen(doc), &err);
    CHECK(s != NULL, "open");
    if (!s) {
        printf("%d FAILED\n", ++fails);
        return 1;
    }

    csson_rule *root = csson_root(s);
    CHECK(eqf(csson_selector_text(root), "cssonv1"), "root selectorText");
    CHECK(csson_rule_count(root) == 2, "root has 2 child nodes");

    csson_rule *d0 = csson_rule_at(root, 0);
    csson_rule *d1 = csson_rule_at(root, 1);
    CHECK(eqf(csson_selector_text(d0), "dept"), "child 0 type");
    CHECK(eqf(csson_get_property(root, "--org"), "\"Acme\""), "getPropertyValue --org (verbatim)");
    CHECK(eqf(csson_get_property_value(d0, "--n"), "3"), "typed value --n");
    CHECK(eqf(csson_get_property(d0, "n"), "3"), "get without -- prefix");
    CHECK(csson_property_count(d0) == 2, "d0 property count");
    CHECK(eqf(csson_property_name_at(d0, 0), "--name"), "property name item 0");
    CHECK(eqf(csson_get_property_value(d1, "--name"), "\"Ops\""), "d1 name");

    /* setProperty: replace, then create */
    CHECK(csson_set_property(root, "--org", "\"Beta\"", &err) == 0, "set replace");
    CHECK(eqf(csson_get_property_value(root, "--org"), "\"Beta\""), "after set replace");
    CHECK(csson_set_property(d0, "--lead", "\"Alice\"", &err) == 0, "set create");
    CHECK(eqf(csson_get_property_value(d0, "--lead"), "\"Alice\""), "after set create");

    /* removeProperty */
    CHECK(csson_remove_property(d0, "--name", &err) == 0, "remove property");
    CHECK(csson_get_property(d0, "--name") == NULL, "property gone");

    /* insertRule (append) then deleteRule */
    CHECK(csson_insert_rule(root, "team { --x: 1; }", -1, &err) == 0, "insert rule");
    CHECK(csson_rule_count(root) == 3, "count after insert");
    CHECK(csson_delete_rule(root, 0, &err) == 0, "delete rule 0");
    CHECK(csson_rule_count(root) == 2, "count after delete");

    /* the live source still parses to canonical JSON */
    char *text = csson_sheet_text(s);
    char *canon = csson_to_canonical_json(text, strlen(text), &err);
    CHECK(canon != NULL && strstr(canon, "\"org\":\"Beta\"") && strstr(canon, "\"team\""),
          "edited sheet canonicalizes");
    csson_free_string(text);
    csson_free_string(canon);

    csson_close(s); /* frees all handles */

    if (fails) {
        printf("%d FAILED\n", fails);
        return 1;
    }
    printf("sheet_test: all passed\n");
    return 0;
}
