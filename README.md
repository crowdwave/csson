# CSSON

**CSSON is a data format which is a strict subset of CSS.**

It is a configuration/data format that reuses a strict subset of CSS syntax instead of inventing a new one.

The core idea is:

```css
app {
  --environment: production;
  --port: 8080;
  --timeout: 30s;
  --regions: us, eu, apac;

  database {
    --engine: postgres;
    --pool-min: 2;
    --pool-max: 10;
  }
}
```

which parses to something like:

```json
{
  "app": {
    "environment": "production",
    "port": 8080,
    "timeout": { "value": 30, "unit": "s" },
    "regions": ["us", "eu", "apac"],
    "database": {
      "engine": "postgres",
      "pool-min": 2,
      "pool-max": 10
    }
  }
}
```

## CSSON in one sentence

**CSSON is CSS used as a typed, comment-friendly, browser-loadable config format, where records are CSS blocks and fields are custom properties.**

## Main rules

### Records are CSS blocks

```css
app {
  ...
}

server {
  ...
}
```

The selector name becomes the object name.

### Fields are custom properties

```css
--port: 8080;
--timeout: 30s;
--environment: production;
```

Only `--name: value;` declarations count as data fields.

Normal CSS properties such as this are rejected in strict CSSON:

```css
color: red;
font-size: 12px;
```

### Nested objects are nested blocks

```css
server {
  --host: api.example.com;

  database {
    --engine: postgres;
    --pool-max: 10;
  }

  cache {
    --ttl: 30s;
  }
}
```

This becomes:

```json
{
  "server": {
    "host": "api.example.com",
    "database": {
      "engine": "postgres",
      "pool-max": 10
    },
    "cache": {
      "ttl": { "value": 30, "unit": "s" }
    }
  }
}
```

You can also allow CSS-nesting-style child forms:

```css
& .database {
  --engine: postgres;
}
```

but the meaning is still just “child object named `database`”.

### Arrays are comma-separated values

```css
--regions: us, eu, apac;
--ports: 5432, 6432, 7432;
```

These become arrays.

### Tuples are space-separated values

```css
--capture-size: 1920px 1080px;
--range: 1 100;
```

These are fixed-shape tuples, ideally validated by schema.

### Comments are native

```css
/* This is a production config */
app {
  --port: 8080; /* public HTTP port */
}
```

This is one of the major advantages over JSON.

## Types CSSON naturally gets from CSS

CSSON can use CSS value syntax for:

```text
strings / bare text
identifiers / enums
integers
numbers
percentages
ratios
lengths: px, rem, vw, cm, mm, etc.
times: ms, s
angles: deg, rad, turn
frequencies: Hz, kHz
resolutions: dpi, dppx
colors: #fff, rgb(...), oklch(...)
URLs: url(...)
comma lists
space tuples
```

So this is valid CSSON-style data:

```css
app {
  --name: Aurora Console;
  --port: 8080;
  --scale: 1.5;
  --rollout: 25%;
  --aspect: 16 / 9;
  --timeout: 30s;
  --padding: 16px;
  --angle: 0.25turn;
  --tone: 440Hz;
  --dpr: 2dppx;
  --brand: #ff6a3d;
  --accent: oklch(0.6 0.15 250);
  --docs: url("https://example.com/docs");
}
```

## Schema with `@property`

CSSON can use a strict `@property` subset as schema:

```css
@property --environment {
  syntax: "development | staging | production";
  inherits: false;
  initial-value: development;
}

@property --port {
  syntax: "<integer>";
  inherits: false;
  initial-value: 8080;
}

@property --timeout {
  syntax: "<time>";
  inherits: false;
  initial-value: 30s;
}

@property --regions {
  syntax: "<custom-ident>#";
  inherits: false;
  initial-value: us;
}
```

Meaning:

```text
--environment must be one of development/staging/production
--port must be an integer
--timeout must be a CSS time value
--regions must be a comma-separated list of identifiers
```

For CSSON, invalid values should be **hard validation errors**, not silently ignored or replaced.

## What strict CSSON rejects

To keep it deterministic and server-friendly, strict CSSON should reject browser-dependent CSS features:

```css
--x: var(--other);
--x: calc(100% - 10px);
--x: env(safe-area-inset-top);
--x: attr(data-x);
--x: inherit;
--x: initial;
--x: unset;
--x: revert;
```

It should also reject arbitrary CSS selectors and at-rules:

```css
@media (...) {}
@supports (...) {}
@import "...";
body {}
.app {}
#app {}
app:hover {}
app, server {}
```

The aim is not “all CSS as config”. The aim is **a small, safe, deterministic data subset of CSS**.

## Browser loading

A CSSON file can be loaded by the browser as a stylesheet if it is valid CSS and served as:

```http
Content-Type: text/css
```

The file does **not** need a `.css` suffix. This can work:

```html
<link rel="stylesheet" href="/config.csson">
```

provided the server serves it as `text/css`.

But for application logic, the better pattern is usually:

```js
const source = await fetch("/config.csson").then(r => r.text());
const config = validateAndParseCsson(source);
```

That lets your own CSSON validator enforce the strict data rules.

## Best short definition

CSSON is:

```text
CSS comments
+ @property schemas
+ simple record blocks
+ custom-property fields
+ nested child blocks
+ CSS scalar value syntax
+ comma arrays
+ space tuples
+ strict validation
```

It is not:

```text
a full CSS engine
a styling language
a cascade system
a browser-computed config format
JSON with different punctuation
```

The strongest version of CSSON is a **CSS-syntax config format** with a Rust/Servo-based reference validator, CLI, WASM package, C ABI, and language bindings.

---

**Structured data that is also valid CSS.** A node is a CSS style rule, fields are
custom properties (`--key`), nested objects are nested rules, and repeated sibling
rules become arrays. The point is to carry structured data through pipelines that
already parse CSS — without adding a second format or parser.

This repository's goal: provide everything needed to use CSSON across the
**major programming environments**, from a single standard that every
implementation reads the same way.

## Prime directive
> **CSSON never writes its own parsing/processing.** Every implementation reads
> from a real CSS parser's authored rule tree and only maps that tree to data. No
> hand-written tokenizers, scanners, brace-walkers, or regex extraction. The
> system is based on **liblexbor**; the reference core is C over lexbor. See
> `CLAUDE.md`.

## Layout
```
csson/
  CLAUDE.md         project directives (prime directive above)
  spec/             the standard — the single canonical definition (versioned)
    v1/SPEC.md      normative CSSON v1 (current)
  core/             the C reference core on liblexbor: libcsson (read + edit) + the csson CLI
                    include/csson.h (C ABI), src/, cli/, build.sh, PLATFORMS.md
  packages/
    typescript/     the JS/TS environment (wraps a real CSS parser)
  bindings/         one wrapper per environment, each links libcsson (or lexbor-via-WASM)
    wasm/ nodejs/ rust/ python/ go/ java/ dotnet/ c/ ruby/ php/ shell/
  conformance/      canonical document + expected JSON + independent multi-engine verifier
    v1/readers/     c (lexbor) · chrome (Blink) · firefox (Gecko)
    VERIFICATION.md the cross-engine (core / Chrome / Firefox) verification runbook
  docs/             css-data-complete-guide.html — the complete guide (live demo, open in a browser)
```

## Versioning
- **Standard version** — a single integer (CSSON v1, v2, …); the caller selects it
  (default = current). **v1 is the current — and only — standard.** A future v2
  (if needed) is added beside v1, which is then frozen.
- **Implementation version** — each library's own semver, independent of the
  standard version.

## Architectures
amd64 (x86-64) and ARM64 are first-class across Linux/macOS/Windows, plus wasm.
CPU architecture never affects semantics. See `core/PLATFORMS.md`.

## Status
- `spec/v1` — current standard (draft).
- `conformance/v1` — canonical doc + `expected.json` (614 B, MD5 `9ae94e…`) plus
  a numeric edge-case fixture. Byte-identical across **C (lexbor)**, **Chrome
  (Blink)**, **Firefox (Gecko)**, and the core — incl. floats/exponents. The
  browser reader uses **only built-in CSSOM** (no deps); `packages/typescript`
  ships it for end users.
- `core` — **working**: `libcsson` + the `csson` CLI on lexbor. Reads canonical
  JSON (matches `expected.json`) and performs **comment-preserving** edits
  (`set`/`rm`) and **RFC 6902 JSON Patch** (`patch`, atomic) via byte-splicing at
  lexbor offsets. Patch JSON parsed by vendored yyjson (MIT, stack-safe).
- `packages/typescript`, `bindings/*` — scaffolds; each links `libcsson` (or
  lexbor compiled to WASM) for its environment.

See `docs/css-data-complete-guide.html` — the complete CSSON v1 guide; open it in
any browser, the demo reads CSSON live via the built-in CSSOM (no server, no deps).
