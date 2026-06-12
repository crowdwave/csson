# S2 — `@property` validation spike (csstree/validator in QuickJS)

**Question:** can `@property` value validation be done **in QuickJS** via csstree's
lexer, and does it **agree with the browser**?

## Verdict: ✅ PASS (including browser parity)

Three-way comparison over 17 cases spanning `<integer>`, `<color>`, `<length>`,
`<percentage>`, `<custom-ident>`, a `|` union, and `+` / `#` multipliers:

| Source | Result |
|---|---|
| **csstree `lexer.match(syntax,value)` in QuickJS** | **17/17 correct** |
| **Chrome native `CSS.registerProperty`** (the oracle) | **17/17, identical verdicts** |

⇒ **csstree == browser** on every case. The mechanism: `csstree.lexer.match(syntaxString, value)`
returns `{ matched }`; `matched !== null` means the value satisfies the `@property`
`syntax` descriptor. The browser oracle: `CSS.registerProperty({syntax, initialValue:value})`
throws iff the value doesn't match.

## Why this matters

This is the capability the **C/lexbor core cannot do** (no value-definition-syntax
matcher) and that we previously scoped as "bind a Rust engine." It turns out to be
an **off-the-shelf MIT library** (`css-tree`'s lexer / `csstree/validator`) that
**runs in QuickJS and matches the browser**. So on the QuickJS path, `csson_validate`
is essentially:
```js
const r = csstree.lexer.match(syntaxDescriptor, value);
return r.matched !== null;   // valid iff matched
```

## Engine / size

- Loaded `node_modules/css-tree/dist/csstree.js` — a **self-contained IIFE**, 202 KB,
  **0 `createRequire`** (mdn-data inlined). Runs as-is in QuickJS.
- Read + `@property` ≈ QuickJS ~490 KB + csstree dist ~200 KB (smaller as bytecode)
  ≈ **~700 KB**. (`csstree/validator`'s own dist, or a trimmed build dropping the
  bundled source-map decoder, would be smaller.)

## Caveats (honest)

- **17 cases, not exhaustive.** Representative across types/unions/multipliers, but
  a shipping validator must test the full `@property` type set (image, url, angle,
  time, resolution, length-percentage, transform-function/list) and tricky values
  (`calc()`, browser-specific functions), where mdn-data-driven csstree *could*
  diverge from a specific browser. **Gate against the Chrome/Firefox oracle** in the
  conformance harness (as the plan already requires).
- This validates *values against a syntax*; it does not yet read the `@property`
  rules out of a CSSON document — that's the parser side (S1 path), trivial to add.

## Full type-set sweep (follow-up to the 17 cases)

Swept **62 cases** across every `@property` type (length, number, percentage,
length-percentage, color, image, url, integer, angle, time, resolution,
transform-function, transform-list, custom-ident, string) + unions + multipliers
+ calc + invalid descriptors, csstree vs Chrome.

- **Field-value validation: 61/62 == browser.** The full grammar is covered.
- The 2 apparent "misses" (`2em`, `calc(10px+2em)` as `<length>`) were **oracle
  artifacts**: I used `registerProperty(initialValue)` which also enforces the
  `@property` **computationally-independent initial-value rule** (font-relative
  units rejected). Confirmed via `CSSStyleValue.parse`: `2em` *is* a valid `<length>`
  **value** (browser true) — only invalid as an **initial-value**. csstree was right.
- The **one real gap**: the **universal `*`** syntax — csstree returns no-match;
  the browser accepts anything. **Fix: 2-line special-case** (`if (syntax==='*') return true`).

### Two `@property` behaviors beyond field-value matching
1. **Universal `*`** → trivial special-case (above).
2. **`initial-value` must be computationally independent** (no font/viewport-relative
   units, no `var()`/`env()`) — a real `@property` constraint the browser enforces
   and csstree's grammar match does not. Only affects validating the `@property`
   declaration's *own* initial-value (optional/advisory in CSSON), not field values.
   The browser is the only exact oracle for it.

## Repro
`spikes/s2/` cases; `css-tree@3.2.1` `dist/csstree.js` concatenated with the test
harness, run in `quickjs-ng` `qjs`; browser oracle via headless Chrome (CDP)
`CSS.registerProperty`.
