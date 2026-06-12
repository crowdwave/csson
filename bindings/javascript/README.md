# CSSON for JavaScript / TypeScript

Read, edit, and validate [CSSON](../../README.md) — structured data that is also
valid CSS — from Node.js, Deno, or the browser. Runs the CSSON core (QuickJS +
PostCSS + csstree) compiled to **WebAssembly**, so it needs no native build and
its output is byte-identical to the C library and the browsers. Written in
TypeScript; ships types.

## Install / build

The package runs `csson.wasm` (the portable core). In this repo:

```sh
cd bindings/javascript
npm install
npm run build       # compiles TS -> dist/ and copies ../wasm/csson.wasm here
# (npm run wasm rebuilds csson.wasm from source first; needs wasi-sdk)
```

## Use — Node.js

The engine loads lazily; the API is ready to call (synchronous):

```ts
import csson from "@csson/js";
// or: import { parse, get, set, patch, validate } from "@csson/js";

const text = `cssonv1 {
  --org: "Acme";          /* comments are preserved by edits */
  dept { --name: "Eng"; --n: 3; }
  dept { --name: "Ops"; }
}`;

csson.parse(text);                // { org: "Acme", dept: [ { n: 3, name: "Eng" }, … ] }
csson.get(text, "/dept/0/n");     // 3
csson.set(text, "/org", "Beta");  // new source text, comment intact
csson.remove(text, "/dept/1");    // new source text
csson.patch(text, [{ op: "replace", path: "/org", value: "Z" }]);
csson.stringify({ app: { port: 8080 } });    // CSSON document
csson.validate("<integer>", "5");             // true  (CSS @property check)
csson.version();                              // "1"
```

## Use — browser / Deno

Loading wasm is async, so initialize once:

```ts
import { init } from "@csson/js/browser";
const csson = await init(new URL("./csson.wasm", import.meta.url));
csson.parse(text);
```

> In the browser, **editing** must go through this wasm core: the native CSSOM
> exposes no source byte offsets and editing via `cssText` strips comments. The
> wasm result is byte-identical to the native library (browser-parity invariant).

## API

| Method | Returns | Notes |
|---|---|---|
| `parse(text)` | value | parse to canonical JSON |
| `get(text, pointer)` | value | RFC 6901 pointer; `""` = whole doc |
| `set(text, pointer, value)` | `string` | edit from a JS value (comment-preserving) |
| `setRaw(text, pointer, token)` | `string` | edit from a raw CSSON token (advanced) |
| `remove(text, pointer)` | `string` | delete a field/node |
| `patch(text, ops)` | `string` | RFC 6902 JSON Patch (atomic) |
| `stringify(obj)` | `string` | object → CSSON |
| `validate(syntax, value)` | `boolean` | `@property` value check |
| `version()` | `string` | supported standard version |

Invalid input throws `CssonError`.

## Notes on types
Coercion is deliberate for cross-engine determinism: a bare integer becomes a
number, a quoted string becomes a string, and **everything else (decimals like
`1.5`, units like `30s`, colors, `url(...)`) is a string** — exact and lossless.
Every named block is a JSON **array** (a single block is a one-element list),
mirroring the browser CSSOM. See the top-level README and `samples/features/`.

## Test
```sh
npm test        # node test.mjs
```
