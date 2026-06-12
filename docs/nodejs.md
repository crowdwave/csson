[← all docs](../README.md)

# CSSON for Node.js

**Read, edit and validate CSSON from Node.js with `@csson/js`.**

The package runs the CSSON core (QuickJS + PostCSS + csstree) compiled to
**WebAssembly**, so it needs *no native build* and its output is byte-identical to the
C library, Python and the browsers. Written in TypeScript; ships its types. Files use the
`.css` extension, named `<name>-csson.css`.

## Install — nothing to compile

The compiled `dist/` and the portable `csson.wasm` are **shipped in the package**, so
there is no build step and no native toolchain — just import it:

```js
import csson from "@csson/js";   // works immediately
```

> The package is **ESM** (`"type": "module"`), targets **Node ≥ 18** (which provides the
> built-in `node:wasi` used to load the core), and has **no runtime dependencies**. No
> native addon, no compiler toolchain, no build at install time.

(Only if you edit the TypeScript source do you rebuild: `cd bindings/javascript && npm
install && npm run build`. `npm run wasm` rebuilds `csson.wasm` from source, which needs
wasi-sdk.)

**Verify it works** — you should see exactly this:

```js
import csson from "@csson/js";
console.log(csson.version());                    // 1
console.log(csson.parse("cssonv1{ --x: 1; }"));  // { x: 1 }
```

## Usage

The engine loads lazily on first use, so the API is **synchronous** and ready to call — no
`await init()` on Node.

```js
import csson from "@csson/js";
// or: import { parse, get, set, patch, validate } from "@csson/js";

const text = `cssonv1 {
  --org: "Acme";          /* comments are preserved by edits */
  dept { --name: "Eng"; --n: 3; }
}`;

csson.parse(text);                 // { org: "Acme", dept: [ { n: 3, name: "Eng" } ] }
csson.get(text, "/dept/0/n");      // 3
csson.set(text, "/org", "Beta");   // new source text, comment intact
csson.patch(text, [{ op: "replace", path: "/org", value: "Z" }]);
csson.stringify({ app: { port: 8080 } });    // CSSON document
csson.validate("<integer>", "5");            // true  (CSS @property check)
```

Read it from disk like any text file — for example
`fs.readFileSync("config-csson.css", "utf8")` — then pass the string to `parse`. Editing
functions (`set`, `remove`, `patch`) return new source text, which you write back yourself.

## API

Every method is exported both as a named function and on the default export. Invalid input
throws `CssonError`.

| Method | Returns | Note |
|---|---|---|
| `parse(text)` | `Json` | parse a CSSON document to its canonical JSON value |
| `get(text, pointer)` | `Json` | read one value at an RFC 6901 JSON Pointer (`""` = whole doc) |
| `set(text, pointer, value)` | `string` | replace the scalar at `pointer` from a JS value; returns new source (comments kept) |
| `setRaw(text, pointer, token)` | `string` | replace the scalar from a raw CSSON token (advanced); returns new source |
| `remove(text, pointer)` | `string` | delete the field/node at `pointer`; returns new source |
| `patch(text, ops)` | `string` | apply an RFC 6902 JSON Patch (atomic); returns new source |
| `stringify(obj)` | `string` | serialize a JS object to a CSSON document (inverse of parse) |
| `validate(syntax, value)` | `boolean` | true if `value` matches a CSS `@property` `syntax` (e.g. `"<integer>"`) |
| `version()` | `string` | the supported CSSON standard version (e.g. `"1"`) |

## TypeScript

Types ship with the package — the `Json` value type and the `Csson` API interface are
exported, so no `@types` install is needed.

```ts
import csson, { type Json, type Csson } from "@csson/js";

const data: Json = csson.parse(text);
```

> A bare integer becomes a JSON **number**; a quoted string becomes a **string**;
> **everything else** (decimals like `1.5`, units like `30s`, colors, `url(...)`) is kept
> as an exact string. Every named block is a JSON **array** (a single block is a
> one-element list), mirroring the browser CSSOM.

---

See also the [JavaScript/TypeScript binding](../bindings/javascript/) and its
[README](../bindings/javascript/README.md).
