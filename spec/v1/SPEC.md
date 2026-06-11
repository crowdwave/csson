# CSSON v1 — normative specification

**Version:** 1.0 · **Status:** Draft

CSSON stores JSON-like structured data (objects, nested trees) inside a file that
is also valid CSS. The point is not to compete with JSON in general; it is to
carry structured data through pipelines that **already** parse CSS — design-token
systems, themeable apps, anything with a stylesheet parser on hand — without
adding a second format or parser.

This version is deliberately tight, and deliberately small: it exposes only what
a real CSS parser already gives you, and invents nothing on top of CSS.

---

## 0. File extension & media type

A CSSON file uses the **`.csson`** extension. Because a CSSON file is also valid
CSS, it is served as **`Content-Type: text/css`** so a browser will parse it as a
stylesheet (e.g. `<link rel="stylesheet" href="data.csson">`). Tools read `.csson`
files directly.

## 1. Data model

A CSSON document encodes one tree of **nodes**. Each node has:

- **fields** — named scalars, and
- **child nodes**, which may repeat (an object array).

The scalar types are: **string**, **integer**, and **identifier** (an unquoted
token treated as a string). There are no value-arrays: a field holds one scalar.
Lists of things are expressed natively, as repeated child nodes.

---

## 2. Canonical syntax (normative)

**The document root is a single top-level style rule whose selector is
`cssonv1`.** This rule is required and its name is fixed: a conforming reader
locates the `cssonv1` rule among the stylesheet's top-level rules (any other
top-level rule — e.g. `@property` — is ignored) and rejects a document that has
none. The root rule's declarations are the root object's fields and its nested
rules are the root's children; the root rule itself produces no key in the output.
The version travels in the root name (`cssonv1` → CSSON v1), so the document is
self-identifying and no separate marker field is needed.

1. **A node is a CSS style rule.** Nesting expresses the tree: a node's children
   are style rules nested in its block.
2. **The node type is the rule's selector**, serialized, with a leading `&` (and
   surrounding whitespace) stripped. Selectors are type tags; they are not meant
   to match any element.
3. **Fields are custom properties only** — every key is written `--key`. Bare CSS
   properties are forbidden, because engines silently discard unknown ones.
4. **An object array is repeated sibling rules of the same type**, kept in source
   order. A child type always maps to a JSON array, even with one member.
5. **Scalars only in values.** A field value is a single scalar (§6). `{ }`,
   `( )`, and `[ ]` must not be used to group values — CSSON does not define any
   value-grouping syntax. To express a list, use repeated child nodes (§2.4).
6. **Scalar forms:** a string is double-quoted (`"Bob"`); an integer is an
   optional `-` then digits (`3`, `-7`); anything else unquoted is an identifier
   and is read as a string (`active`).

### Example

```css
@property --org    { syntax: "<string>";       inherits: false; initial-value: ""; }
@property --name   { syntax: "<string>";       inherits: false; initial-value: ""; }
@property --lead   { syntax: "<string>";       inherits: false; initial-value: ""; }
@property --level  { syntax: "<integer>";      inherits: false; initial-value: 0; }
@property --id     { syntax: "<integer>";      inherits: false; initial-value: 0; }
@property --status { syntax: "<custom-ident>"; inherits: false; initial-value: unknown; }

cssonv1 {
  --org: "Acme";
  department {
    --name: "Engineering";
    team {
      --name: "Platform"; --lead: "Alice";
      member {
        --name: "Bob"; --level: 3;
        role { --name: "backend"; }
        role { --name: "infra"; }
        project { --id: 101; --status: active; }
        project { --id: 102; --status: archived; }
      }
      member {
        --name: "Carol"; --level: 2;
        role { --name: "frontend"; }
        project { --id: 103; --status: active; }
      }
    }
    team {
      --name: "Data"; --lead: "Dan";
      member {
        --name: "Eve"; --level: 4;
        role { --name: "ml"; }
        role { --name: "infra"; }
        project { --id: 201; --status: active; }
      }
    }
  }
  department {
    --name: "Design";
    team {
      --name: "Brand"; --lead: "Faye";
      member {
        --name: "Gil"; --level: 2;
        role { --name: "visual"; }
      }
    }
  }
}
```

---

## 3. Schema (`@property`)

An `@property` rule per field declares an **inline schema**: the scalar type —
`<string>`, `<integer>`, `<custom-ident>`. The schema travels inside the document,
which bare JSON cannot do.

`@property` is optional and advisory: it does not change how a conforming reader
parses structure, only how a reader may validate or strongly type scalars.

---

## 4. Read path (normative)

CSSON is read from the **authored rule tree** produced by a real CSS parser:
lexbor's CSSOM, the browser's `cssRules`, or an equivalent.

**CSSON implementations never write their own CSS parsing** (tokenizers, scanners,
brace-walkers, regex extraction). This is the project's prime directive.

Reading via `getComputedStyle` or the Typed OM is **non-conformant**: the cascade
collapses duplicate property names and discards source order. Conforming readers
walk the authored rule tree only.

---

## 5. Value coercion

A reader turns each field value into a typed scalar. The input MUST be the
**verbatim source token** of the declaration value — a reader must not normalize
it (no number reformatting, no precision change). This is what lets independent
engines and browsers agree byte-for-byte (e.g. `1e3` stays `"1e3"`, and
`3.14159265358979` keeps full precision). Then:

1. Trim surrounding whitespace.
2. The token is an **integer** if, and only if, it is a *canonical* integer
   literal **and** in the JSON-safe range:
   - it matches `0` or `-?[1-9][0-9]*` (no leading zeros, no `-0`, no sign-only,
     no `+`, no decimal point, no exponent), **and**
   - its absolute value is `≤ 2^53 − 1` (9007199254740991).

   This is exactly JavaScript's `Number.isSafeInteger`, so the C core and the
   browser agree on which tokens become numbers. Tokens that look numeric but
   fail either test (`007`, `-0`, `1e3`, `3.14`, `9007199254740992`,
   `99999999999999999999`) stay **strings**, verbatim.
3. Double-quoted → **string** with the quotes removed.
4. Anything else → **string** (the token verbatim — floats, units, percentages,
   exponents, identifiers).

A conforming reader reads the verbatim token from the parser's source offsets
(lexbor) or the CSSOM `getPropertyValue` (browsers), not a re-parsed numeric
value.

**JSON escaping.** When a scalar is emitted as a JSON string, the reader escapes
it per RFC 8259: `"` and `\` are backslash-escaped, the C0 control characters use
their short escapes (`\b \f \n \t \r`) or `\u00XX` otherwise. An integer is
emitted verbatim (it is already canonical), never re-formatted.

---

## 6. Canonical JSON

To compare or hash documents, a reader emits **canonical JSON**:

- objects use the coerced field values and the child arrays, keyed by node type;
- **object keys are sorted**;
- output is **compact** (no insignificant whitespace);
- object arrays preserve source order.

The example in §2 canonicalizes to (single line, wrapped here):

```json
{"department":[{"name":"Engineering","team":[{"lead":"Alice","member":[{"level":3,
"name":"Bob","project":[{"id":101,"status":"active"},{"id":102,"status":"archived"}],
"role":[{"name":"backend"},{"name":"infra"}]},{"level":2,"name":"Carol","project":
[{"id":103,"status":"active"}],"role":[{"name":"frontend"}]}],"name":"Platform"},
{"lead":"Dan","member":[{"level":4,"name":"Eve","project":[{"id":201,
"status":"active"}],"role":[{"name":"ml"},{"name":"infra"}]}],"name":"Data"}]},
{"name":"Design","team":[{"lead":"Faye","member":[{"level":2,"name":"Gil","role":
[{"name":"visual"}]}],"name":"Brand"}]}],"org":"Acme"}
```

Canonical form: 614 bytes, MD5 `9ae94e393bf09a58bb597acee1b5d975`.

---

## 7. Conformance & cross-engine result

A reader is conforming if, for any document following §2, it produces the §6
canonical JSON via the §4 read path.

The example was run through independent engines, each emitting canonical JSON:

| Reader | Engine | Path |
|---|---|---|
| C | lexbor 3.1.0 | CSSOM rule-tree walk (verbatim value via source offsets) |
| Chrome | Blink (chrome-remote-interface / CDP) | `cssRules` walk |
| Firefox | Gecko (geckodriver / WebDriver) | `cssRules` walk |

The C core and **both browsers** (Chrome and Firefox) produce **byte-identical**
output, including numeric edge cases (`1e3`, full-precision floats) — the
browser-parity invariant holds. Because array-ness is gone, keys are custom
properties read from the rule tree, the read path is the authored rule tree, and
values are the verbatim token, the engines have nothing left to disagree about.

---

## 8. Scope & limitations

- **Positioning.** CSSON earns its place only where CSS is already the transport.
  For greenfield structured data, JSON/TOML/YAML have deeper tooling.
- **Maturity.** There is no broad ecosystem, schema registry, or editor support.
- **Type selectors match nothing** by design; CSSON nodes are inert as far as
  styling is concerned, which is intended.

---

## Appendix A — fields summary

| Construct | CSS form | JSON result |
|---|---|---|
| Node | `type { … }` (nested style rule) | object |
| Node type | selector, leading `&` stripped | object key under parent |
| Field | `--key: value;` | `"key": value` |
| String | `"text"` | `"text"` |
| Integer | `3`, `-7` | `3`, `-7` |
| Identifier | `active` | `"active"` |
| Object array | repeated `type { … }` siblings | `[ {…}, {…} ]` |
