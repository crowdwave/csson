/* api_test.c — exercises the JSON-bridge API (get / from_json / set_json) and the
 * error-kind classifier directly against the C ABI. Run under ctest. */
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

int main(void) {
    const char *doc = "cssonv1 {\n --org: \"Acme\";\n dept { --name: \"Eng\"; --n: 3; }\n}\n";
    size_t dl = strlen(doc);
    char *err = NULL, *out = NULL;

    /* get: scalar, integer, whole document, not-found */
    out = csson_get(doc, dl, "/org", &err);
    CHECK(eq(out, "\"Acme\""), "get /org");
    csson_free_string(out);

    out = csson_get(doc, dl, "/dept/0/n", &err);
    CHECK(eq(out, "3"), "get /dept/0/n integer");
    csson_free_string(out);

    out = csson_get(doc, dl, "", &err);
    CHECK(out && strstr(out, "\"org\":\"Acme\""), "get root via empty pointer");
    csson_free_string(out);

    err = NULL;
    out = csson_get(doc, dl, "/nope", &err);
    CHECK(out == NULL, "get not-found returns NULL");
    CHECK(csson_error_kind_of(err) == CSSON_E_NOT_FOUND, "kind NOT_FOUND");
    csson_free_string(err);

    /* from_json: round-trips through the read path back to canonical JSON */
    const char *j = "{\"org\":\"Acme\",\"dept\":[{\"name\":\"Eng\",\"size\":12}]}";
    err = NULL;
    char *cs = csson_from_json(j, strlen(j), &err);
    CHECK(cs != NULL, "from_json produces a document");
    char *back = cs ? csson_to_canonical_json(cs, strlen(cs), &err) : NULL;
    CHECK(eq(back, "{\"dept\":[{\"name\":\"Eng\",\"size\":12}],\"org\":\"Acme\"}"),
          "from_json | canon round-trip");
    csson_free_string(cs);
    csson_free_string(back);

    err = NULL;
    cs = csson_from_json("nope", 4, &err);
    CHECK(cs == NULL && csson_error_kind_of(err) == CSSON_E_PARSE, "from_json bad input -> PARSE");
    csson_free_string(err);

    /* set_json: JSON value (no manual quoting); object rejected as non-scalar */
    err = NULL;
    char *e = csson_set_json(doc, dl, "/org", "\"Beta\"", &err);
    CHECK(e != NULL, "set_json succeeds");
    char *v = e ? csson_get(e, strlen(e), "/org", &err) : NULL;
    CHECK(eq(v, "\"Beta\""), "set_json wrote the value");
    csson_free_string(v);
    csson_free_string(e);

    err = NULL;
    e = csson_set_json(doc, dl, "/org", "{\"a\":1}", &err);
    CHECK(e == NULL && csson_error_kind_of(err) == CSSON_E_VALUE, "set_json object -> VALUE");
    csson_free_string(err);

    /* error kinds for the read path */
    err = NULL;
    out = csson_to_canonical_json("body{--x:1;}", 12, &err);
    CHECK(out == NULL && csson_error_kind_of(err) == CSSON_E_NO_ROOT, "kind NO_ROOT");
    csson_free_string(err);

    if (fails) {
        printf("%d FAILED\n", fails);
        return 1;
    }
    printf("api_test: all passed\n");
    return 0;
}
