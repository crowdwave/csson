/* csson — the CSSON command-line utility (over libcsson / liblexbor).
 *
 *   csson canon     <file>               parse -> canonical JSON (default verb)
 *   csson get       <file> <ptr>         read one value (JSON) at a JSON Pointer
 *   csson check     <file>               exit 0 if valid CSSON, else 1
 *   csson set       <file> <ptr> <value> replace a scalar from a raw CSSON token
 *   csson set-json  <file> <ptr> <json>  replace a scalar from a JSON value
 *   csson rm        <file> <ptr>         remove a field or node (comment-preserving)
 *   csson patch     <file> <patch.json>  apply an RFC 6902 JSON Patch (comment-preserving)
 *   csson from-json <file>               serialize a JSON object into a CSSON document
 *   csson versions                       standard versions this build supports
 *
 * <file> of "-" reads stdin. Read/edit output goes to stdout (pipe or redirect).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/csson.h"

static_assert(__STDC_VERSION__ >= 202311L, "CSSON requires C23 (compile with -std=c23)");

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

/* Hard ceiling on input size (a .csson document or a patch). Bounds memory use on
   a huge or endless stream rather than growing until the process is killed. */
#define CSSON_MAX_INPUT ((size_t)256 << 20) /* 256 MiB */

static char *slurp(const char *path, size_t *len) {
    FILE *f = (strcmp(path, "-") == 0) ? stdin : fopen(path, "rb");
    if (!f) {
        perror("open");
        exit(1);
    }
    size_t cap = 1 << 16, n = 0;
    char *b = xmalloc(cap);
    for (size_t r; (r = fread(b + n, 1, cap - n, f)) > 0;) {
        n += r;
        if (n > CSSON_MAX_INPUT) {
            fprintf(stderr, "csson: input exceeds %zu bytes\n", (size_t)CSSON_MAX_INPUT);
            exit(1);
        }
        if (n == cap) {
            cap *= 2;
            b = xrealloc(b, cap);
        }
    }
    if (f != stdin)
        fclose(f);
    b[n] = 0;
    *len = n;
    return b;
}

/* report an error message (consumes it) and yield exit code 1 */
static int fail(char *err) {
    fprintf(stderr, "csson: %s\n", err ? err : "error");
    csson_free_string(err);
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr,
                "usage: csson canon|get|check|set|set-json|rm|patch|from-json|versions ...\n");
        return 2;
    }
    const char *cmd = argv[1];

    if (strcmp(cmd, "versions") == 0) {
        printf("%s\n", csson_supported_versions());
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr, "csson %s: needs <file>\n", cmd);
        return 2;
    }

    size_t len;
    char *src = slurp(argv[2], &len);
    char *err = nullptr, *out = nullptr, *patch = nullptr;
    int rc = 0;

    if (strcmp(cmd, "canon") == 0) {
        out = csson_to_canonical_json(src, len, &err);
        if (!out)
            rc = fail(err);
        else
            printf("%s\n", out);
    } else if (strcmp(cmd, "get") == 0) {
        if (argc < 4) {
            fprintf(stderr, "csson get <file> <pointer>\n");
            rc = 2;
        } else {
            out = csson_get(src, len, argv[3], &err);
            if (!out)
                rc = fail(err);
            else
                printf("%s\n", out);
        }
    } else if (strcmp(cmd, "from-json") == 0) {
        out = csson_from_json(src, len, &err);
        if (!out)
            rc = fail(err);
        else
            fputs(out, stdout);
    } else if (strcmp(cmd, "check") == 0) {
        out = csson_to_canonical_json(src, len, &err);
        if (!out) {
            fprintf(stderr, "csson: invalid: %s\n", err ? err : "");
            csson_free_string(err);
            rc = 1;
        }
    } else if (strcmp(cmd, "set") == 0) {
        if (argc < 5) {
            fprintf(stderr, "csson set <file> <pointer> <value>\n");
            rc = 2;
        } else {
            out = csson_set(src, len, argv[3], argv[4], &err);
            if (!out)
                rc = fail(err);
            else
                fputs(out, stdout);
        }
    } else if (strcmp(cmd, "set-json") == 0) {
        if (argc < 5) {
            fprintf(stderr, "csson set-json <file> <pointer> <json-value>\n");
            rc = 2;
        } else {
            out = csson_set_json(src, len, argv[3], argv[4], &err);
            if (!out)
                rc = fail(err);
            else
                fputs(out, stdout);
        }
    } else if (strcmp(cmd, "rm") == 0) {
        if (argc < 4) {
            fprintf(stderr, "csson rm <file> <pointer>\n");
            rc = 2;
        } else {
            out = csson_remove(src, len, argv[3], &err);
            if (!out)
                rc = fail(err);
            else
                fputs(out, stdout);
        }
    } else if (strcmp(cmd, "patch") == 0) {
        if (argc < 4) {
            fprintf(stderr, "csson patch <file> <patch.json>\n");
            rc = 2;
        } else {
            size_t plen;
            patch = slurp(argv[3], &plen);
            out = csson_patch(src, len, patch, &err);
            if (!out)
                rc = fail(err);
            else
                fputs(out, stdout);
        }
    } else {
        fprintf(stderr, "csson: unknown command '%s'\n", cmd);
        rc = 2;
    }

    csson_free_string(out);
    free(patch);
    free(src);
    return rc;
}
