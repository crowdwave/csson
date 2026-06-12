[← all docs](../README.md)

# CSSON CLI

**The `csson` command reads, edits and validates CSSON — a data format which is a strict subset of CSS.**

Point it at a `<name>-csson.css` file (or pipe one in) to get canonical JSON, read a single value by JSON Pointer, set or remove scalars, apply an RFC 6902 patch, or check a value against a CSS `@property` syntax. Edits splice the original source, so comments and formatting survive.

## Install — nothing to compile

A prebuilt, self-contained `csson` (Linux x86-64) ships in [`bin/`](../bin). Put it on your `$PATH`:

```sh
cp bin/csson /usr/local/bin/csson
```

**Verify it works** — you should see exactly this:

```sh
csson --version
# csson 0.2.0 (CSSON standard v1)

echo 'cssonv1{ --x: 1; }' | csson canon -
# {"x":1}
```

### Other platforms / your own build

The prebuilt binary is Linux x86-64. On macOS/Windows or another arch, build from source (see **[Building from source](building.md)**) — the binary lands at `core/build/csson`.

## Usage basics

Every invocation has the form:

```sh
csson <command> [arguments]
```

- A `<file>` of `-` reads from **stdin**, so you can pipe documents in.
- Read and edit output goes to **stdout** — pipe it onward or redirect it to a new file.
- Files follow the `<name>-csson.css` naming convention (e.g. `config-csson.css`).

| Exit code | Meaning |
| --- | --- |
| `0` | success — valid document / value, operation completed |
| `1` | invalid input or the operation failed |
| `2` | usage error (wrong/missing arguments) |

## Commands

| Command | Arguments | Description |
| --- | --- | --- |
| `canon` | `<file>` | parse a document to canonical JSON (default) |
| `get` | `<file> <pointer>` | read one value at an RFC 6901 JSON Pointer |
| `check` | `<file>` | exit 0 if the document is valid CSSON, else 1 |
| `set` | `<file> <pointer> <value>` | replace a scalar from a raw CSSON token |
| `set-json` | `<file> <pointer> <json>` | replace a scalar from a JSON value |
| `rm` | `<file> <pointer>` | remove a field or node (comment-preserving) |
| `patch` | `<file> <patch.json>` | apply an RFC 6902 JSON Patch (comment-preserving) |
| `from-json` | `<file>` | serialize a JSON object into a CSSON document |
| `validate` | `<syntax> <value>` | check a value against a CSS `@property` syntax |
| `versions` | | CSSON standard versions this build supports |

### Worked examples

Parse a document to canonical JSON:

```sh
csson canon config-csson.css
```

Read a single value by JSON Pointer, piping the document in on stdin:

```sh
cat config-csson.css | csson get - /server/0/port
```

Replace a scalar from a JSON value and write a new file:

```sh
csson set-json config-csson.css /port 9090 > new-csson.css
```

Replace a scalar from a raw CSSON token:

```sh
csson set config-csson.css /timeout 30s > new-csson.css
```

Remove a field or node (the first element of the `old` array here):

```sh
csson rm config-csson.css /old/0 > new-csson.css
```

Apply an RFC 6902 JSON Patch:

```sh
echo '[{"op":"replace","path":"/env","value":"prod"}]' > patch.json && csson patch config-csson.css patch.json
```

Check a value against a CSS `@property` syntax (prints `true`/`false`, exit `0`/`1`):

```sh
csson validate '<integer>' 5
# true
```

Confirm a document is valid CSSON without producing output:

```sh
csson check config-csson.css   # exit 0 if valid, 1 if not
```

## Options

| Option | Description |
| --- | --- |
| `-h`, `--help` | show the help and exit |
| `-V`, `--version` | show the version and exit |

> Edits are **comment-preserving**. The CLI splices the original source bytes at the parser-provided offsets rather than re-serializing, so `set`, `set-json`, `rm` and `patch` keep your comments and formatting intact.
