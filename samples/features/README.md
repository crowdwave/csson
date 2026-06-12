# CSSON feature samples

Feature-by-feature examples, each at graduated complexity (`simple` →
`intermediate` → `advanced`). Filenames are `feature-<feature>-<level>-<n>-csson.css`.
Every file opens with a comment block explaining what it teaches and the
expected canonical JSON, and is heavily commented inline. All are valid CSSON v1
(`csson check` passes). Read the JSON any file produces with:

```sh
csson canon samples/features/feature-arrays-simple-1-csson.css
```

> Note on canonical shape: in CSSON v1 **every named block becomes a JSON array**
> — a block that appears once is a one-element array (`server { } →
> {"server":[{…}]}`). Repeating a block name just adds elements. The files'
> comments reflect this real shape.

## records — objects as named blocks (the core structure)
`feature-records-simple-1` · `feature-records-simple-2` · `feature-records-intermediate-1` · `feature-records-advanced-1`

## fields — `--key: value;` custom-property data
`feature-fields-simple-1` · `feature-fields-simple-2` · `feature-fields-intermediate-1` (duplicate → **last wins**) · `feature-fields-advanced-1` (edge-case key names)

## arrays — repeated sibling blocks → JSON arrays
`feature-arrays-simple-1` · `feature-arrays-simple-2` · `feature-arrays-intermediate-1` (single block = 1-elem array; order preserved) · `feature-arrays-intermediate-2` (several arrays) · `feature-arrays-advanced-1` (nested arrays) · `feature-arrays-advanced-2` (14-element playlist)

## nesting — nested blocks → nested objects (depth)
`feature-nesting-simple-1` · `feature-nesting-simple-2` · `feature-nesting-intermediate-1` (4 levels) · `feature-nesting-advanced-1` (6 levels) · `feature-nesting-advanced-2` (nesting + arrays at 3 depths)

## css-nesting — the `& child` form
`feature-css-nesting-simple-1` · `feature-css-nesting-intermediate-1` · `feature-css-nesting-advanced-1` (parity: a plain block and `& block` of the same name array together)

## scalars — value coercion / types
`feature-scalars-simple-1` (int→number, "quoted"→string, ident→string) · `feature-scalars-simple-2` (gotchas: `1.5`/`true`/`null` stay **strings**) · `feature-scalars-intermediate-1` (unit catalogue) · `feature-scalars-advanced-1` (colors, url, ratios, tuples)

## floats — floating-point values (a decimal is a **string**, not a JSON number)
`feature-floats-simple-1` (integer→number vs decimal→string) · `feature-floats-simple-2` (every float form: `.5`, `1.`, `1e3`, `-0.0`, exact text kept) · `feature-floats-intermediate-1` (real config + the integer-cents pattern for money) · `feature-floats-advanced-1` (precision guarantee: arbitrary precision, trailing zeros, signed zero; the cross-engine reason)

## schema — `@property` typing (metadata, absent from data output)
`feature-schema-simple-1` · `feature-schema-simple-2` · `feature-schema-intermediate-1` · `feature-schema-advanced-1` (multiplier/combinator syntaxes) · `feature-schema-advanced-2` (full type catalogue + nested/arrays)

## comments — native CSS comments as documentation
`feature-comments-simple-1` · `feature-comments-intermediate-1` · `feature-comments-advanced-1` (intensely documented; comments survive `csson set`/`patch` edits)

## tuples — space-separated fixed-shape values (a single verbatim **string**)
`feature-tuples-simple-1` · `feature-tuples-intermediate-1` · `feature-tuples-advanced-1` (tuple string vs separate fields vs nested object)

## comma-lists — comma-separated values (a single verbatim **string**, NOT a JSON array)
`feature-lists-simple-1` · `feature-lists-intermediate-1` · `feature-lists-advanced-1` (when to use a comma-list string vs a repeated-block array)

---
**44 files** spanning 11 features. The two idiom features (`tuples`, `lists`) are
single string scalars by design — for true arrays use repeated blocks (`arrays`).
Decimals are strings too (`floats`) — for a JSON number use a bare integer.
