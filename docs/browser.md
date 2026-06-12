[← all docs](../README.md)

# CSSON in the browser

In the browser, **read** CSSON with the native CSSOM — no dependencies, no wasm — and **edit** it (set / remove / patch) with the same wasm core as everywhere else, byte-identical.

## Verify it works

Copy the prebuilt `browser.js` and `csson.wasm` from [`bindings/javascript/`](../bindings/javascript/) next to this page, serve the folder over HTTP (ES modules and wasm don't load from `file://` — e.g. `npx serve`), and open it. The console should print exactly `1` and `{ x: 1 }`:

```html
<!-- verify.html -->
<script type="module">
  import { init } from "./browser.js";
  const csson = await init(new URL("./csson.wasm", import.meta.url));
  console.log(csson.version());                    // 1
  console.log(csson.parse("cssonv1{ --x: 1; }"));  // { x: 1 }
</script>
```

For **reading only** you don't even need the wasm — the browser parses CSSON natively; see [Read via the native CSSOM](#read-via-the-native-cssom-no-wasm-no-deps) below.

## Serve it as CSS

A CSSON file *is* valid CSS, so the browser parses it natively — but only when the server hands it over as a stylesheet. A CSSON file is named `<name>-csson.css` and must be served with `Content-Type: text/css`. Once it is, you can load it like any stylesheet:

```html
<link rel="stylesheet" href="/config-csson.css">
```

…or fetch its text and build a sheet from it yourself:

```js
const text = await fetch("/config-csson.css").then(r => r.text());
```

> The `-csson.css` name and the `text/css` MIME type are what make a browser parse the file with its native CSS engine. Reading then needs nothing else.

## Read via the native CSSOM (no wasm, no deps)

The browser has already parsed the stylesheet into its authored rule tree, so reading is just a walk of `cssRules`: find the `cssonv1` rule, map each `--custom` property to a field, and turn nested rules into children (stripping a leading `&` from the selector). This is the same algorithm as the C/lexbor core and the other engines — it is **read-only** and produces byte-identical canonical data.

```js
// Read-only canonical reader using ONLY the browser's built-in CSSOM.
function cssonCanon(sheet) {
  const seltype = r => r.selectorText.replace(/^[&\s]+/, "").trim();
  const coerce = s => {
    s = s.trim();
    // canonical integer in the JS-safe range → number; otherwise verbatim string
    if (/^(?:0|-?[1-9][0-9]*)$/.test(s)) {
      const n = Number(s);
      if (Number.isSafeInteger(n)) return n;
      return s;
    }
    if (s.length >= 2 && s[0] === '"' && s.at(-1) === '"') return s.slice(1, -1);
    return s;
  };
  const node = rule => {
    const o = {}, st = rule.style;
    for (let i = 0; i < st.length; i++) {
      const p = st[i];
      if (p.startsWith("--")) o[p.slice(2)] = coerce(st.getPropertyValue(p));
    }
    for (const r of rule.cssRules || []) {
      if (r.selectorText === undefined) continue;   // skip @property etc.
      const ty = seltype(r);
      (o[ty] = o[ty] || []).push(node(r));
    }
    return o;
  };
  let result = null;
  for (const r of sheet.cssRules) {
    if (r.selectorText !== undefined && seltype(r) === "cssonv1") result = node(r);
  }
  // sort keys recursively for a canonical, byte-identical shape
  const canon = v => {
    if (Array.isArray(v)) return v.map(canon);
    if (v && typeof v === "object") {
      const out = {};
      for (const k of Object.keys(v).sort()) out[k] = canon(v[k]);
      return out;
    }
    return v;
  };
  return JSON.stringify(canon(result));
}
```

If you have a loaded `<link>` stylesheet, grab its `CSSStyleSheet` from `document.styleSheets` and pass it straight to `cssonCanon`. If you only have the text, build a sheet from it first:

```js
const sheet = new CSSStyleSheet();
await sheet.replace(cssonText);
// …then walk sheet.cssRules, e.g. cssonCanon(sheet)
```

> This path is **read-only**. It is byte-identical to the C (lexbor) reader and the other browser engines — that parity is exactly what the conformance harness checks. See the reference browser reader at [`../conformance/v1/readers/browser/reader.js`](../conformance/v1/readers/browser/reader.js).

## Edit via the wasm core

To **set**, **remove**, **patch**, or to author / validate, load the wasm binding. Loading wasm is async, so initialize once and reuse the returned API:

```js
import { init } from "@csson/js/browser";

const csson = await init(new URL("./csson.wasm", import.meta.url));

csson.parse(text);                  // canonical JSON (same as the reader above)
csson.set(text, "/port", 9090);     // comment-preserving — returns new source text
csson.remove(text, "/dept/1");      // returns new source text
csson.validate("<integer>", "5");   // true — CSS @property value check
```

> Editing must go through the wasm core, not the native CSSOM. The CSSOM exposes **no source byte offsets**, so it cannot splice the original text, and editing through `cssText` re-serializes the rules and **strips comments**. The wasm core edits by splicing the original bytes at the parser-provided offsets, so comments and formatting survive — and its output is byte-identical to the native C library (the browser-parity invariant). Reading is dependency-free; editing brings in the one wasm download.

The wasm binding lives at [`../bindings/javascript/`](../bindings/javascript/).

## Types & shape

> A bare integer becomes a JSON **number**; a quoted string becomes a **string**; **everything else** (decimals like `1.5`, units like `30s`, colors, `url(...)`) is kept as an exact string. Every named block is a JSON **array** — a single block is a one-element list. That is exactly how the browser CSSOM models rules (repeated selectors are just repeated rules), which is why CSSON reads identically here and everywhere else.
