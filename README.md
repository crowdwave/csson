# CSSON

**CSSON is a data format which is a strict subset of CSS.**

> **Is this for real?** Sort of — it's a bit of fun: a thought experiment, a
> weekend project that got out of hand.

## Implementations

Read, edit and validate CSSON from any of these — they all share one core, so every one gives identical results. Full guides in [`docs/`](docs/):

| | |
|---|---|
| **[CLI](docs/cli.md)** | the `csson` command — read / edit / validate from a shell |
| **[Node.js / TypeScript](docs/nodejs.md)** | [`@csson/js`](bindings/javascript) — runs the WebAssembly core; no native build |
| **[Python](docs/python.md)** | [`csson`](bindings/python) — `ctypes` over `libcsson`; zero dependencies |
| **[Browser](docs/browser.md)** | read via the native CSSOM, edit via the WebAssembly core |

## Examples

**Practical examples** — [`samples/showcase/`](samples/showcase/):

[3d-scene](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/3d-scene-csson.css) · [board-game-rules](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/board-game-rules-csson.css) · [brewery-recipe](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/brewery-recipe-csson.css) · [cargo-ship](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/cargo-ship-csson.css) · [cdn-edge](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/cdn-edge-csson.css) · [chemistry-lab](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/chemistry-lab-csson.css) · [ci-pipeline](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/ci-pipeline-csson.css) · [database-schema](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/database-schema-csson.css) · [design-tokens](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/design-tokens-csson.css) · [dns-zone](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/dns-zone-csson.css) · [drone-mission](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/drone-mission-csson.css) · [ecommerce-catalog](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/ecommerce-catalog-csson.css) · [esports-tournament](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/esports-tournament-csson.css) · [financial-portfolio](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/financial-portfolio-csson.css) · [firewall-rules](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/firewall-rules-csson.css) · [fleet-logistics](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/fleet-logistics-csson.css) · [flight-plan](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/flight-plan-csson.css) · [genome-pipeline](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/genome-pipeline-csson.css) · [greenhouse-automation](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/greenhouse-automation-csson.css) · [hello-minimal](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/hello-minimal-csson.css) · [hospital-ward](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/hospital-ward-csson.css) · [kubernetes-deployment](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/kubernetes-deployment-csson.css) · [library-catalog](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/library-catalog-csson.css) · [load-balancer](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/load-balancer-csson.css) · [mars-rover](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/mars-rover-csson.css) · [message-queue](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/message-queue-csson.css) · [ml-hyperparameters](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/ml-hyperparameters-csson.css) · [movie-shotlist](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/movie-shotlist-csson.css) · [nuclear-reactor](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/nuclear-reactor-csson.css) · [nutrition-plan](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/nutrition-plan-csson.css) · [observability-stack](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/observability-stack-csson.css) · [orchestra](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/orchestra-csson.css) · [particle-detector](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/particle-detector-csson.css) · [podcast-feed](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/podcast-feed-csson.css) · [railway-timetable](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/railway-timetable-csson.css) · [recipe-lasagna](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/recipe-lasagna-csson.css) · [retro-emulator](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/retro-emulator-csson.css) · [robot-arm](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/robot-arm-csson.css) · [rpg-character](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/rpg-character-csson.css) · [satellite-constellation](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/satellite-constellation-csson.css) · [smart-home](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/smart-home-csson.css) · [solar-power-plant](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/solar-power-plant-csson.css) · [spacecraft-mission](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/spacecraft-mission-csson.css) · [submarine-systems](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/submarine-systems-csson.css) · [synth-patch](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/synth-patch-csson.css) · [theme-park](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/theme-park-csson.css) · [traffic-intersection](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/traffic-intersection-csson.css) · [weather-station](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/weather-station-csson.css) · [web-server](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/web-server-csson.css) · [wind-turbine-farm](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/showcase/wind-turbine-farm-csson.css)

**Feature demonstrations** — [`samples/features/`](samples/features/) (each teaches one capability, simple → advanced):

[feature-arrays-advanced-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-arrays-advanced-1-csson.css) · [feature-arrays-advanced-2](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-arrays-advanced-2-csson.css) · [feature-arrays-intermediate-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-arrays-intermediate-1-csson.css) · [feature-arrays-intermediate-2](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-arrays-intermediate-2-csson.css) · [feature-arrays-simple-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-arrays-simple-1-csson.css) · [feature-arrays-simple-2](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-arrays-simple-2-csson.css) · [feature-comments-advanced-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-comments-advanced-1-csson.css) · [feature-comments-intermediate-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-comments-intermediate-1-csson.css) · [feature-comments-simple-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-comments-simple-1-csson.css) · [feature-css-nesting-advanced-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-css-nesting-advanced-1-csson.css) · [feature-css-nesting-intermediate-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-css-nesting-intermediate-1-csson.css) · [feature-css-nesting-simple-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-css-nesting-simple-1-csson.css) · [feature-fields-advanced-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-fields-advanced-1-csson.css) · [feature-fields-intermediate-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-fields-intermediate-1-csson.css) · [feature-fields-simple-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-fields-simple-1-csson.css) · [feature-fields-simple-2](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-fields-simple-2-csson.css) · [feature-floats-advanced-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-floats-advanced-1-csson.css) · [feature-floats-intermediate-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-floats-intermediate-1-csson.css) · [feature-floats-simple-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-floats-simple-1-csson.css) · [feature-floats-simple-2](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-floats-simple-2-csson.css) · [feature-lists-advanced-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-lists-advanced-1-csson.css) · [feature-lists-intermediate-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-lists-intermediate-1-csson.css) · [feature-lists-simple-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-lists-simple-1-csson.css) · [feature-nesting-advanced-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-nesting-advanced-1-csson.css) · [feature-nesting-advanced-2](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-nesting-advanced-2-csson.css) · [feature-nesting-intermediate-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-nesting-intermediate-1-csson.css) · [feature-nesting-simple-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-nesting-simple-1-csson.css) · [feature-nesting-simple-2](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-nesting-simple-2-csson.css) · [feature-records-advanced-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-records-advanced-1-csson.css) · [feature-records-intermediate-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-records-intermediate-1-csson.css) · [feature-records-simple-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-records-simple-1-csson.css) · [feature-records-simple-2](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-records-simple-2-csson.css) · [feature-scalars-advanced-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-scalars-advanced-1-csson.css) · [feature-scalars-intermediate-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-scalars-intermediate-1-csson.css) · [feature-scalars-simple-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-scalars-simple-1-csson.css) · [feature-scalars-simple-2](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-scalars-simple-2-csson.css) · [feature-schema-advanced-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-schema-advanced-1-csson.css) · [feature-schema-advanced-2](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-schema-advanced-2-csson.css) · [feature-schema-intermediate-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-schema-intermediate-1-csson.css) · [feature-schema-simple-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-schema-simple-1-csson.css) · [feature-schema-simple-2](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-schema-simple-2-csson.css) · [feature-tuples-advanced-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-tuples-advanced-1-csson.css) · [feature-tuples-intermediate-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-tuples-intermediate-1-csson.css) · [feature-tuples-simple-1](https://raw.githubusercontent.com/crowdwave/csson/refs/heads/main/samples/features/feature-tuples-simple-1-csson.css)

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

This becomes (each block an array, per *rules, not styles*):

```json
{
  "server": [
    {
      "host": "api.example.com",
      "database": [ { "engine": "postgres", "pool-max": 10 } ],
      "cache": [ { "ttl": "30s" } ]
    }
  ]
}
```

You can also allow CSS-nesting-style child forms:

```css
& .database {
  --engine: postgres;
}
```

but the meaning is still just “child object named `database`”.

### Repeating a block makes an array

The real way to get a JSON array is to **repeat a block** — each occurrence is one
element (exactly how `cssRules` lists rules):

```css
server { --host: "a"; }
server { --host: "b"; }
```

```json
{ "server": [ { "host": "a" }, { "host": "b" } ] }
```

### Comma and space lists are single string values

A comma- or space-separated value is **not** split — it is kept verbatim as one
string, a compact idiom you split where you consume it:

```css
--regions: us, eu, apac;        /* -> "us, eu, apac"   (one string, not an array) */
--capture-size: 1920px 1080px;  /* -> "1920px 1080px"  (a fixed-shape tuple)      */
```

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

In the JSON, **only `--port: 8080` becomes a number** — every other value above is
kept as its exact string (`"1.5"`, `"25%"`, `"30s"`, `"#ff6a3d"`,
`"oklch(0.6 0.15 250)"`, …). The CSS *type* each value claims is what `@property`
validates, below; CSSON stores the value, not a parsed breakdown of it.

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

> **Status:** this is the strict-mode *goal* and what the spec defines. The
> current reader is lenient — it keeps such values as plain strings and treats
> such selectors as record names rather than erroring; an enforcing strict-mode
> validator is planned.

## Browser loading

A CSSON file can be loaded by the browser as a stylesheet if it is valid CSS and served as:

```http
Content-Type: text/css
```

The file does **not** need a `.css` suffix. This can work:

```html
<link rel="stylesheet" href="/config-csson.css">
```

provided the server serves it as `text/css`.

But for application logic, the better pattern is usually:

```js
const source = await fetch("/config-csson.css").then(r => r.text());
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

The strongest version of CSSON is a **CSS-syntax config format**.

---

## Quick taste

```sh
# CLI: a CSSON file IS a stylesheet — parse it to canonical JSON
csson canon config-csson.css
csson get config-csson.css /database/0/pool-max     # one value, by JSON Pointer
csson set-json config-csson.css /port 9090          # comment-preserving edit
csson validate '<integer>' 8080                     # @property check
```
```js
// Node.js / TypeScript
import csson from "@csson/js";
csson.parse(text);                  // -> canonical JSON
csson.set(text, "/port", 9090);     // -> new source, comments intact
```
```python
# Python
import csson
csson.loads(text)                   # -> dict
csson.patch(text, [{"op": "replace", "path": "/env", "value": "prod"}])
```

## CSSON is CSS *rules*, not CSS *styles*

This is the key idea. A stylesheet is a set of **rules** — `selector { property: value }`
blocks — which a browser then *applies* to a page, resolving the cascade,
specificity and inheritance into the **computed styles** on each element.

**CSSON only ever uses the rules as written** — the authored block-and-declaration
tree, the exact thing the browser exposes as `document.styleSheets` → `cssRules`.
It never applies them to anything: there is no page, no element, no cascade, no
computed value. A CSSON document is the *shape* of a stylesheet, read as data.

That single choice explains everything:

- **A selector is a name, not a match.** `database { … }` is a record *called*
  "database", not a rule that targets `<database>` elements.
- **A declaration is a field, not a style.** `--port: 8080` is data, not paint.
- **Every block is a list item → a JSON array.** A stylesheet's rules are an
  ordered *list*, so each block becomes an array; a single `database { }` is a
  one-element array `[{…}]`, and repeating the block name just adds items. This is
  exactly how `cssRules` behaves — CSSON matches the browser, byte for byte.
- **No cascade ⇒ deterministic.** No specificity, no `!important`, no inheritance;
  order is literal. A duplicate field is plain last-wins by source order, and the
  result is identical on every engine.

The core idea is (every document is wrapped in a single `cssonv1` root):

```css
cssonv1 {
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
}
```

which reads as this canonical JSON:

```json
{
  "app": [
    {
      "environment": "production",
      "port": 8080,
      "timeout": "30s",
      "regions": "us, eu, apac",
      "database": [
        { "engine": "postgres", "pool-min": 2, "pool-max": 10 }
      ]
    }
  ]
}
```

Note the two consequences of *rules, not styles* (above): every block is an
**array** (`app` and `database` are one-element lists), and only a bare integer
is a JSON number — `8080` stays `8080`, but `30s` and `us, eu, apac` are kept as
exact **strings**. (Canonical output also sorts keys; shown here in source order.)
