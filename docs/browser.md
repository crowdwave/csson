[← all docs](../README.md)

# CSSON in the browser

Here's the fun part: **you need nothing to use CSSON in the browser.** A CSSON file
is *just CSS*, and every browser already has a world-class CSS parser built in — so
the browser parses your CSSON for you, and you read the data straight out of the
stylesheet. No library, no build, no dependency.

We **also** ship a small WebAssembly build of the CSSON core. You don't need it to
*read* CSSON — but it's easier, and it adds two things the bare browser can't do:
**editing** (set / remove / patch, with comments preserved) and **`@property`
validation**.

So there are two paths. Pick by what you need — both take you all the way to a
plain JavaScript object:

| | **Path A** — just the browser | **Path B** — the CSSON wasm |
|---|---|---|
| Needs | nothing (built-in CSSOM) | `csson.wasm` + a tiny loader |
| Read data | ✅ | ✅ |
| Edit (comments preserved) | ❌ | ✅ |
| Validate `@property` | ❌ | ✅ |
| Byte-identical to every other CSSON engine | ✅ | ✅ |

---

## Path A — pure browser, zero dependencies

The browser parses the file as a stylesheet; you walk its rules into a data object.
About 20 lines, nothing to install.

### Step 1 — have a CSSON file, served as CSS

A CSSON file is named `<name>-csson.css` and must be served with
`Content-Type: text/css` so the browser treats it as a stylesheet. Example
`config-csson.css`:

```css
cssonv1 {
  app {
    --name: "Aurora";
    --port: 8080;
    database { --engine: postgres; --pool-max: 10; }
  }
}
```

### Step 2 — get it into a `CSSStyleSheet`

Either load it as a real stylesheet…

```html
<link rel="stylesheet" href="config-csson.css">
```
```js
const sheet = [...document.styleSheets].find(s => s.href?.endsWith("config-csson.css"));
```

…or fetch the text and build a sheet yourself (works for any string):

```js
const text  = await fetch("config-csson.css").then(r => r.text());
const sheet = new CSSStyleSheet();
sheet.replaceSync(text);
```

### Step 3 — paste this reader (browser built-ins only)

This is the entire reader. It finds the `cssonv1` root, turns `--custom` properties
into fields and nested rules into child arrays, and coerces values — exactly
matching every other CSSON engine:

```js
function cssonRead(sheet) {
  const seltype = r => r.selectorText.replace(/^[&\s]+/, "").trim();
  const coerce = s => {
    s = s.trim();
    if (/^(?:0|-?[1-9][0-9]*)$/.test(s)) {                 // a plain integer -> number
      const n = Number(s);
      return Number.isSafeInteger(n) ? n : s;
    }
    if (s.length >= 2 && s[0] === '"' && s.at(-1) === '"')  // "quoted" -> string
      return s.slice(1, -1);
    return s;                                               // everything else -> exact string
  };
  const node = rule => {
    const o = {};
    for (const p of rule.style)
      if (p.startsWith("--")) o[p.slice(2)] = coerce(rule.style.getPropertyValue(p));
    for (const r of rule.cssRules ?? []) {
      if (r.selectorText === undefined) continue;           // skip @property, etc.
      const ty = seltype(r);
      (o[ty] ??= []).push(node(r));                         // repeated blocks -> array
    }
    return o;
  };
  for (const r of sheet.cssRules)
    if (r.selectorText !== undefined && seltype(r) === "cssonv1") return node(r);
  throw new Error("no cssonv1 root rule");
}
```

### Step 4 — read your data

```js
const data = cssonRead(sheet);
console.log(data.app[0].port);                 // 8080
console.log(data.app[0].database[0].engine);   // "postgres"
```

Done — your CSSON is now a plain JavaScript object, with **zero dependencies**.

> **Path A is read-only.** It can't *edit* a file and keep its comments: the CSSOM
> exposes no source byte offsets, and writing back via `cssText` strips comments.
> For editing, use Path B.

---

## Path B — the CSSON wasm (easier; reads, edits, validates)

The prebuilt `csson.wasm` plus a tiny loader give you the full API — and nothing to
compile.

### Step 1 — get the files

Copy the prebuilt `csson.wasm` and `browser.js` from
[`bindings/javascript/`](../bindings/javascript/) onto your site. Serve over HTTP —
ES modules and wasm don't load from `file://` (any static server works, e.g.
`npx serve`).

### Step 2 — initialise once

```js
import { init } from "./browser.js";
const csson = await init(new URL("./csson.wasm", import.meta.url));
```

### Step 3 — read your data (and more)

```js
const text = await fetch("config-csson.css").then(r => r.text());

const data = csson.parse(text);
console.log(data.app[0].port);                 // 8080
console.log(data.app[0].database[0].engine);   // "postgres"

// the extras the bare browser can't do:
const edited = csson.set(text, "/app/0/port", 9090);  // -> new source, comments kept
csson.validate("<integer>", "9090");                   // -> true
```

### Verify it works

Put `browser.js`, `csson.wasm` and this page in one folder, serve it (`npx serve`),
and open it — the console should print exactly `1` then `{ app: [ { port: 8080 } ] }`:

```html
<script type="module">
  import { init } from "./browser.js";
  const csson = await init(new URL("./csson.wasm", import.meta.url));
  console.log(csson.version());                                  // 1
  console.log(csson.parse("cssonv1{ app{ --port: 8080; } }"));   // { app: [ { port: 8080 } ] }
</script>
```

---

## Which path should I use?

- **Just reading config?** Path A. This is the whole point of CSSON — your data is
  already parsed by the browser, for free, with nothing to ship.
- **Editing, validating, or you want one identical API across Node, Python, the CLI
  and the browser?** Path B — the same core everywhere, byte for byte.

## Types & shape (both paths)

A bare integer becomes a JSON **number**; a `"quoted"` value a **string**; and
**everything else** (decimals like `1.5`, units like `30s`, colors, `url(...)`) is
kept as an **exact string**. Every named block is a JSON **array** — a single block
is a one-element list — mirroring how the browser's CSSOM lists rules. See the
[project README](../README.md) for the full rules.
