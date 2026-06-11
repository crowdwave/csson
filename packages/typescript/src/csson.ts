/* csson.ts — CSSON reader for the browser, using ONLY the built-in CSSOM.
 *
 * No dependencies, no bundled parser: an end user loads a .csson file as a
 * stylesheet and this walks the browser's authored `cssRules` tree. Same
 * algorithm as the lexbor core (core/src/read.c) and the shared
 * conformance reader (conformance/v1/readers/browser/reader.js); proven
 * byte-identical across Chrome (Blink) and Firefox (Gecko).
 *
 * Usage:
 *   // 1. via a <link rel="stylesheet" href="data.csson"> already in the page:
 *   import { toCanonicalJson } from "csson";
 *   const json = toCanonicalJson(document.styleSheets[0]);
 *
 *   // 2. from fetched text (constructable stylesheet):
 *   import { fromText } from "csson";
 *   const json = fromText(await (await fetch("data.csson")).text());
 */

export type Scalar = string | number;
export type CssonValue = Scalar | CssonValue[] | { [k: string]: CssonValue };

const selType = (sel: string): string => sel.replace(/^[&\s]+/, "").trim();

function coerce(raw: string): Scalar {
  const s = raw.trim();
  // canonical integer (no leading zeros / "-0") within the JS-safe range → number;
  // otherwise verbatim string. Matches the lexbor core exactly.
  if (/^(?:0|-?[1-9][0-9]*)$/.test(s)) {
    const num = Number(s);
    if (Number.isSafeInteger(num)) return num;
    return s;
  }
  if (s.length >= 2 && s[0] === '"' && s[s.length - 1] === '"') return s.slice(1, -1);
  return s;
}

function nodeOf(rule: any): { [k: string]: CssonValue } {
  const o: { [k: string]: CssonValue } = {};
  const st: CSSStyleDeclaration = rule.style;
  for (let i = 0; i < st.length; i++) {
    const p = st[i];
    if (p.startsWith("--")) o[p.slice(2)] = coerce(st.getPropertyValue(p));
  }
  const kids = rule.cssRules || [];
  for (let i = 0; i < kids.length; i++) {
    const r = kids[i];
    if (r.selectorText === undefined) continue;            // skip @property etc.
    const ty = selType(r.selectorText);
    const arr = (o[ty] as CssonValue[]) || (o[ty] = []);
    arr.push(nodeOf(r));
  }
  return o;
}

function canon(v: CssonValue): CssonValue {
  if (Array.isArray(v)) return v.map(canon);
  if (v && typeof v === "object") {
    const out: { [k: string]: CssonValue } = {};
    for (const k of Object.keys(v).sort()) out[k] = canon(v[k]);
    return out;
  }
  return v;
}

/** Walk a stylesheet's authored rule tree → canonical JSON for the root `cssonv1` node. */
export function toCanonicalJson(sheet: CSSStyleSheet): string {
  let result: CssonValue | null = null;
  const rules = sheet.cssRules;
  for (let i = 0; i < rules.length; i++) {
    const r: any = rules[i];
    if (r.selectorText !== undefined && selType(r.selectorText) === "cssonv1") result = nodeOf(r);
  }
  if (result === null) throw new Error("no root `cssonv1` rule");
  return JSON.stringify(canon(result));
}

/** Build a stylesheet from CSSON text (constructable stylesheet) and read it. */
export function fromText(cssonText: string): string {
  const sheet = new CSSStyleSheet();
  sheet.replaceSync(cssonText);
  return toCanonicalJson(sheet);
}
