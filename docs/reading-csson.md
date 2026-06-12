# Reading CSSON — a guide for end users

CSSON is data that is also valid CSS. This page explains how a CSSON file is
*read*, what that means for you when you **author** one, and the one thing to get
right when you **consume** one in a browser.

The short version:

- **Authoring** a `-csson.css` file requires nothing special — you just write CSS.
- **Reading** a `-csson.css` file must happen from the *authored rule tree* (the rules
  as written), which in a browser means one small, well-defined step.

---

## 1. Authors just write CSS

You may have heard that CSSON reads from the **"authored rule tree."** That sounds
like a rule *you* have to follow — it isn't. It's a description of how a reader
works, not a constraint on how you write.

The authored rule tree literally means *"the rules as you typed them, in the order
you typed them."* When you write:

```css
cssonv1 {
  --name: "Acme";
  team { --lead: "Alice"; }
  team { --lead: "Dan"; }
}
```

…that **is** the authored rule tree. There is no extra annotation, no normalization
step, nothing to comply with. You write nested rules and `--custom` properties, and
the reader hands you back exactly that structure:

```json
{"name":"Acme","team":[{"lead":"Alice"},{"lead":"Dan"}]}
```

If anything, this makes CSSON *easier* to author than you'd expect: **what you type
is what you get.** No cascade, no inheritance, no value rewriting happening behind
your back.

---

## 2. Why it's read this way (and why it can't be otherwise)

A browser exposes a stylesheet two different ways, and CSSON deliberately uses only
one of them.

| View | What it is | Good for CSSON? |
|---|---|---|
| **Authored rule tree** (`cssRules`) | the rules exactly as written | ✅ yes — this is what CSSON reads |
| **Computed style** (`getComputedStyle`) | the final result after the CSS cascade | ❌ no — it destroys the data |

Reading the *computed* view would quietly wreck your data, because the CSS cascade:

- **collapses duplicate properties** — repeated entries would vanish;
- **discards source order** — your lists (object arrays) would lose their order;
- **resolves and reformats values** — exact tokens like `1e3` or a
  full-precision float would get rewritten.

The authored rule tree is the only representation that preserves **structure,
order, and exact scalar values**. That's why CSSON is defined against it, and why
reading via `getComputedStyle` is explicitly non-conformant.

> CSSON reads the authored tree only. It does **not** run the CSS cascade. The one
> exception is cosmetic: if you write the same field twice in one rule, the reader
> keeps the **last** one (matching how a browser would), so the output stays valid.

---

## 3. Reading a CSSON file in the browser

This is the only place there's a small thing to get right. Browsers give you two
*wrong* ways and one *right* way.

### ✅ The right way: fetch the text, read it as data

Fetch the file as text and let the library read it. The TypeScript package does the
work for you:

```js
import { fromText } from "csson";

const text = await fetch("data-csson.css").then(r => r.text());
const json = fromText(text);          // -> canonical JSON string
const data = JSON.parse(json);
```

A handy one-liner you can drop into your own code:

```js
const fromUrl = async (url) => fromText(await fetch(url).then(r => r.text()));
const data = JSON.parse(await fromUrl("data-csson.css"));
```

Under the hood `fromText` builds a *constructable stylesheet*
(`new CSSStyleSheet().replaceSync(text)`) and walks its `cssRules` — the authored
rule tree — using only built-in browser APIs. No bundled parser, no dependencies.

### ❌ Antipattern 1: loading it as a live stylesheet

```html
<!-- don't do this for data you want to READ -->
<link rel="stylesheet" href="data-csson.css">
```

A `<link rel="stylesheet">` tells the browser to **apply** the file as CSS. Two
problems:

- if the file is on **another origin**, accessing its `cssRules` from JavaScript
  throws a `SecurityError` — you can't read it back;
- the browser executes the file's CSS semantics — `@import` is fetched, `url()`
  values trigger requests — so loading **untrusted** CSSON this way is a security
  risk (see `csson-security-analysis.md`, S9).

Read CSSON as *data* (section above); don't attach it to the page.

### ❌ Antipattern 2: reading computed style

```js
// don't do this — it returns the cascaded result, not your data
getComputedStyle(el).getPropertyValue("--name");
```

This gives you the post-cascade value, with duplicates collapsed and order lost.
It is non-conformant and will not round-trip your data.

---

## 4. Reading outside the browser

No constructable-stylesheet step is needed — the reader parses the bytes directly.

**Command line:**

```sh
csson canon data-csson.css      # -> canonical JSON on stdout
```

**Any binding** links the `libcsson` core (or lexbor compiled to WASM); each one
parses with a real CSS parser and walks the authored rule tree, producing
byte-identical JSON to the browser. That cross-engine equality (C core ↔ Chrome ↔
Firefox) is CSSON's core guarantee.

---

## 5. Takeaways

- **Writing CSSON:** just write CSS. The "authored rule tree" is not something you
  do — it's what you already wrote.
- **Reading CSSON in a browser:** `fromText(await fetch(url).then(r => r.text()))`.
  Don't `<link>` it, don't read computed style.
- **Reading anywhere else:** `csson canon file` or the equivalent binding call.
- **Why:** the authored rule tree is the only view that preserves your structure,
  order, and exact values — so CSSON is faithful and identical across every engine.
