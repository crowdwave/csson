# csson (browser)

Read a `.csson` file in the browser using **only the browser's built-in CSSOM** —
no parser, no dependencies. A `.csson` file is valid CSS, so the browser parses
it for you; this module walks the authored `cssRules` tree and returns canonical
JSON.

## Use

Load the file as a stylesheet, then read it:

```html
<link rel="stylesheet" href="data.csson" />
<script type="module">
  import { toCanonicalJson } from "csson";
  const json = toCanonicalJson(document.styleSheets[0]);
</script>
```

Or from fetched text (constructable stylesheet):

```js
import { fromText } from "csson";
const json = fromText(await (await fetch("data.csson")).text());
```

> Serve `.csson` as `Content-Type: text/css` so the browser will parse it as a
> stylesheet.

## What it does

- `toCanonicalJson(sheet: CSSStyleSheet): string` — walk a stylesheet → canonical JSON.
- `fromText(text: string): string` — build a stylesheet from text, then read it.

Canonical JSON = keys sorted, compact, object arrays in source order — identical
to the lexbor `libcsson` core and the `csson` CLI. This is enforced by the
cross-engine conformance suite (`../../conformance/v1`), which runs the **same**
reader logic (`readers/browser/reader.js`) in **Chrome (Blink)** and **Firefox
(Gecko)** and diffs against the canonical `expected.json`.

## No dependencies, by design

This package deliberately uses no CSS parser of its own — the browser is the
parser (the project's prime directive; see `../../CLAUDE.md`). It runs only in a
browser (it needs `CSSStyleSheet`/`cssRules`). For Node / other runtimes, use the
`csson` CLI or the WASM build of the core.

## Build
```
npm install && npm run build   # tsc → dist/csson.js (ESM) + dist/csson.d.ts
```
