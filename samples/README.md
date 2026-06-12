# CSSON samples

Example `.csson` documents — all valid CSSON v1, all using a `.css` extension
(named `*-csson.css`) because CSSON *is* valid CSS. Organised into two sets:

## [`features/`](features/) — learn the format
Feature-by-feature demonstrations at graduated complexity
(`<feature>-simple|intermediate|advanced-<n>-csson.css`), each heavily commented
to explain what it teaches and the canonical JSON it produces. 44 files covering
records, fields, arrays, nesting, css-nesting, scalars, floats, schema (`@property`),
comments, tuples, and comma-lists. **Start here to learn CSSON.**

## [`showcase/`](showcase/) — see it in the wild
50 complete, realistic configs spanning many domains — infrastructure, games &
media, space & transport, science & energy, and everyday/business (from
`hello-minimal` to deeply-nested, schema-typed documents). **Browse here to see
CSSON used for real.**

---
Read any file:
```sh
csson canon samples/showcase/spacecraft-mission-csson.css   # → canonical JSON
csson check samples/features/feature-arrays-simple-1-csson.css      # validate (exit 0 = valid)
```
