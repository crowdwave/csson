/* csson — the CSSON command-line utility (over libcsson).
 *
 * CSSON is a data format which is a strict subset of CSS. CSSON files use the
 * `.css` extension (named `<name>-csson.css`). See `csson --help`.
 *
 * <file> of "-" reads stdin. Read/edit output goes to stdout (pipe or redirect).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/csson.h"

static_assert(__STDC_VERSION__ >= 202311L, "CSSON requires C23 (compile with -std=c23)");

/* The CLI tool version (distinct from the CSSON standard version it implements,
 * which is `csson versions`). */
#define CSSON_CLI_VERSION "0.2.0"

static void usage(FILE *f) {
    fprintf(f,
        "csson " CSSON_CLI_VERSION " — work with CSSON (a data format which is a strict subset of CSS)\n"
        "\n"
        "Usage: csson <command> [arguments]\n"
        "       A <file> of \"-\" reads stdin; read/edit output goes to stdout.\n"
        "\n"
        "Commands:\n"
        "  canon     <file>                     parse a document to canonical JSON (default)\n"
        "  get       <file> <pointer>           read one value at an RFC 6901 JSON Pointer\n"
        "  check     <file>                     exit 0 if the document is valid CSSON, else 1\n"
        "  set       <file> <pointer> <value>   replace a scalar from a raw CSSON token\n"
        "  set-json  <file> <pointer> <json>    replace a scalar from a JSON value\n"
        "  rm        <file> <pointer>           remove a field or node (comment-preserving)\n"
        "  patch     <file> <patch.json>        apply an RFC 6902 JSON Patch (comment-preserving)\n"
        "  from-json <file>                     serialize a JSON object into a CSSON document\n"
        "  validate  <syntax> <value>           check a value against a CSS @property syntax\n"
        "  versions                             CSSON standard versions this build supports\n"
        "\n"
        "Options:\n"
        "  -h, --help                           show this help and exit\n"
        "  -V, --version                        show the version and exit\n"
        "\n"
        "Examples:\n"
        "  csson canon config-csson.css\n"
        "  cat config-csson.css | csson get - /server/0/port\n"
        "  csson set-json config-csson.css /port 9090 > new-csson.css\n"
        "  csson validate '<integer>' 5\n");
}

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

/* Hard ceiling on input size (a -csson.css document or a patch). Bounds memory use on
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
        usage(stderr);
        return 2;
    }
    const char *cmd = argv[1];

    if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "help") == 0) {
        usage(stdout);
        return 0;
    }
    if (strcmp(cmd, "-V") == 0 || strcmp(cmd, "--version") == 0) {
        printf("csson %s (CSSON standard v%s)\n", CSSON_CLI_VERSION, csson_supported_versions());
        return 0;
    }
    if (strcmp(cmd, "versions") == 0) {
        printf("%s\n", csson_supported_versions());
        return 0;
    }
    if (strcmp(cmd, "validate") == 0) { /* validate <syntax> <value> (no file) */
        if (argc < 4) {
            fprintf(stderr, "csson validate <syntax> <value>\n");
            return 2;
        }
        char *verr = nullptr;
        int ok = csson_validate(argv[2], argv[3], &verr);
        if (ok < 0) {
            fprintf(stderr, "csson: %s\n", verr ? verr : "error");
            csson_free_string(verr);
            return 2;
        }
        printf("%s\n", ok ? "true" : "false");
        return ok ? 0 : 1;
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
