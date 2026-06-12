# CSSON feature samples

Feature-by-feature examples, each at graduated complexity (`simple` →
`intermediate` → `advanced`). Filenames are `<feature>-<level>-<n>-csson.css`.
Every file opens with a comment block explaining what it teaches and the
expected canonical JSON, and is heavily commented inline. All are valid CSSON v1
(`csson check` passes). Read the JSON any file produces with:

```sh
csson canon samples/features/arrays-simple-1-csson.css
```

> Note on canonical shape: in CSSON v1 **every named block becomes a JSON array**
> — a block that appears once is a one-element array (`server { } →
> {"server":[{…}]}`). Repeating a block name just adds elements. The files'
> comments reflect this real shape.

## records — objects as named blocks (the core structure)
`records-simple-1` · `records-simple-2` · `records-intermediate-1` · `records-advanced-1`

## fields — `--key: value;` custom-property data
`fields-simple-1` · `fields-simple-2` · `fields-intermediate-1` (duplicate → **last wins**) · `fields-advanced-1` (edge-case key names)

## arrays — repeated sibling blocks → JSON arrays
`arrays-simple-1` · `arrays-simple-2` · `arrays-intermediate-1` (single block = 1-elem array; order preserved) · `arrays-intermediate-2` (several arrays) · `arrays-advanced-1` (nested arrays) · `arrays-advanced-2` (14-element playlist)

## nesting — nested blocks → nested objects (depth)
`nesting-simple-1` · `nesting-simple-2` · `nesting-intermediate-1` (4 levels) · `nesting-advanced-1` (6 levels) · `nesting-advanced-2` (nesting + arrays at 3 depths)

## css-nesting — the `& child` form
`css-nesting-simple-1` · `css-nesting-intermediate-1` · `css-nesting-advanced-1` (parity: a plain block and `& block` of the same name array together)

## scalars — value coercion / types
`scalars-simple-1` (int→number, "quoted"→string, ident→string) · `scalars-simple-2` (gotchas: `1.5`/`true`/`null` stay **strings**) · `scalars-intermediate-1` (unit catalogue) · `scalars-advanced-1` (colors, url, ratios, tuples)

## schema — `@property` typing (metadata, absent from data output)
`schema-simple-1` · `schema-simple-2` · `schema-intermediate-1` · `schema-advanced-1` (multiplier/combinator syntaxes) · `schema-advanced-2` (full type catalogue + nested/arrays)

## comments — native CSS comments as documentation
`comments-simple-1` · `comments-intermediate-1` · `comments-advanced-1` (intensely documented; comments survive `csson set`/`patch` edits)

## tuples — space-separated fixed-shape values (a single verbatim **string**)
`tuples-simple-1` · `tuples-intermediate-1` · `tuples-advanced-1` (tuple string vs separate fields vs nested object)

## comma-lists — comma-separated values (a single verbatim **string**, NOT a JSON array)
`lists-simple-1` · `lists-intermediate-1` · `lists-advanced-1` (when to use a comma-list string vs a repeated-block array)

---
**40 files** spanning 10 features. The two idiom features (`tuples`, `lists`) are
single string scalars by design — for true arrays use repeated blocks (`arrays`).
