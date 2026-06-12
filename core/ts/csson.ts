/* csson.ts — the CSSON reference implementation (TypeScript).
 *
 * Runs inside QuickJS, embedded in libcsson. Parsing + comment-preserving edits
 * use PostCSS; @property validation uses csstree's lexer (global `csstree`,
 * provided by the prepended dist bundle). The C facade calls the functions hung
 * off globalThis.csson; it sets the engine limits (memory/stack/interrupt/size).
 *
 * Per the project rule, all code WE write is TypeScript; the deps are JS.
 */
import postcss, { Root, Rule, Declaration, AnyNode } from 'postcss';

declare const csstree: any; // from the prepended csstree dist bundle

type Json = string | number | boolean | null | Json[] | { [k: string]: Json };

/* ------------------------------------------------------------ scalars ----- */

// CSS Syntax §3.3 input preprocessing on a value token (newlines + NUL), so the
// result matches a browser cssRules walk. Invalid UTF-8 already became U+FFFD when
// the C facade created the JS string. Digits are untouched (1e3 / floats survive).
function preprocess(s: string): string {
  return s.replace(/\r\n?|\f/g, "\n").replace(/\0/g, "�");
}

// Coerce a verbatim value token to a JSON scalar (spec §5).
function coerce(raw: string): string | number {
  const s = preprocess(raw).trim();
  if (/^(?:0|-?[1-9][0-9]*)$/.test(s)) {
    const n = Number(s);
    return Number.isSafeInteger(n) ? n : s;
  }
  if (s.length >= 2 && s[0] === '"' && s[s.length - 1] === '"') return s.slice(1, -1);
  return s;
}

// Split a selector list on TOP-LEVEL commas only (commas inside ()/[] — e.g.
// :is(a, b) — are not list separators). Operates on the already-parsed selector
// token PostCSS handed us, not on raw CSS source.
function splitTopLevel(sel: string): string[] {
  const out: string[] = [];
  let depth = 0;
  let start = 0;
  for (let i = 0; i < sel.length; i++) {
    const c = sel[i];
    if (c === "(" || c === "[") depth++;
    else if (c === ")" || c === "]") depth--;
    else if (c === "," && depth === 0) {
      out.push(sel.slice(start, i));
      start = i + 1;
    }
  }
  out.push(sel.slice(start));
  return out;
}

// Reproduce the browser's nested-rule `selectorText`: CSS Nesting prepends "& "
// to each selector-list item that doesn't already reference the nesting selector,
// exactly as Blink and Gecko serialize cssRules (e.g. `a, b` -> `& a, & b`). The
// browser readers get this for free from the live CSSOM; the core must match it
// so every engine derives the same node key (governing rule: the browser is the
// oracle — see ../../CLAUDE.md).
function browserNestSelector(sel: string): string {
  return splitTopLevel(sel)
    .map((s) => {
      const t = s.trim();
      return t.startsWith("&") ? t : "& " + t;
    })
    .join(", ");
}

// Node type/key: the browser's serialized selector with the leading "&" stripped
// (the same strip the browser readers apply to selectorText).
const seltype = (sel: string): string => browserNestSelector(sel).replace(/^[&\s]+/, "").trim();

// A field key / node type written into CSS must be identifier-safe (the S1/S3
// injection guard): [A-Za-z0-9_-] and non-ASCII only.
function isSafeName(s: string): boolean {
  return s.length > 0 && /^[A-Za-z0-9_-￿-]+$/.test(s);
}

// Serialize a JSON scalar to a CSSON value token (the write-side of coerce).
// Strings are double-quoted with `"` and `\` escaped; numbers verbatim; only
// scalars are representable (objects/arrays/non-integer reals are rejected here).
function scalarToken(v: Json): string {
  if (typeof v === "string") return '"' + v.replace(/[\\"]/g, "\\$&") + '"';
  if (typeof v === "number") {
    if (!Number.isFinite(v)) throw new Error("value not representable in CSSON v1");
    return String(v);
  }
  if (typeof v === "boolean") return v ? "true" : "false";
  if (v === null) return "null";
  throw new Error("value is not a single CSSON scalar");
}

/* ------------------------------------------------------------- read -------- */

function nodeToObject(rule: Rule): { [k: string]: Json } {
  const o: { [k: string]: Json } = {};
  rule.each((child: AnyNode) => {
    if (child.type === "decl" && child.prop.startsWith("--")) {
      o[child.prop.slice(2)] = coerce(child.value); // last-wins (overwrite)
    } else if (child.type === "rule") {
      const ty = seltype(child.selector);
      (o[ty] as Json[]) ??= [];
      (o[ty] as Json[]).push(nodeToObject(child));
    }
  });
  return o;
}

function canon(v: Json): Json {
  if (Array.isArray(v)) return v.map(canon);
  if (v && typeof v === "object") {
    const out: { [k: string]: Json } = {};
    for (const k of Object.keys(v).sort()) out[k] = canon((v as any)[k]);
    return out;
  }
  return v;
}

function rootRule(root: Root): Rule {
  let result: Rule | null = null;
  root.each((n: AnyNode) => {
    if (n.type === "rule" && seltype(n.selector) === "cssonv1") result = n;
  });
  if (!result) throw new Error("no root `cssonv1` rule");
  return result;
}

export function toCanonicalJson(text: string): string {
  return JSON.stringify(canon(nodeToObject(rootRule(postcss.parse(text)))));
}

/* ----------------------------------------------------------- pointer ------- */

const unescapeToken = (t: string): string => t.replace(/~1/g, "/").replace(/~0/g, "~");

function splitPointer(pointer: string): string[] {
  if (pointer === "") return [];
  if (pointer[0] !== "/") throw new Error("invalid JSON Pointer");
  return pointer.split("/").slice(1).map(unescapeToken);
}

function pointerGet(obj: Json, tokens: string[]): Json | undefined {
  let cur: any = obj;
  for (const t of tokens) {
    if (cur == null || typeof cur !== "object") return undefined;
    cur = Array.isArray(cur) ? cur[Number(t)] : cur[t];
  }
  return cur;
}

export function get(text: string, pointer: string): string {
  const v = pointerGet(JSON.parse(toCanonicalJson(text)), splitPointer(pointer));
  if (v === undefined) throw new Error("get: pointer did not resolve");
  return JSON.stringify(v);
}

/* -------------------------------------------------------- @property -------- */

export function validate(syntax: string, value: string): boolean {
  if (syntax.trim() === "*") return true; // universal accepts any value
  return csstree.lexer.match(syntax, value).matched !== null;
}

/* ------------------------------------------------------- navigate (edit) --- */

interface Target {
  rule?: Rule; // a node
  decl?: Declaration; // a field
  parent?: Rule; // the containing rule (for the last token)
}

function childRules(rule: Rule, type: string): Rule[] {
  const out: Rule[] = [];
  rule.each((c: AnyNode) => {
    if (c.type === "rule" && seltype(c.selector) === type) out.push(c);
  });
  return out;
}

function findField(rule: Rule, key: string): Declaration | undefined {
  let d: Declaration | undefined;
  rule.each((c: AnyNode) => {
    if (c.type === "decl" && c.prop === "--" + key) d = c;
  });
  return d;
}

// Resolve tokens against the root rule to a field or node (mirrors the C resolve()).
function navigate(root: Rule, tokens: string[]): Target {
  let rule: Rule = root;
  let i = 0;
  while (i < tokens.length) {
    if (i === tokens.length - 1) {
      const f = findField(rule, tokens[i]);
      if (f) return { decl: f, parent: rule };
      return { parent: rule }; // last token, not a field → maybe a missing key
    }
    const type = tokens[i];
    const idx = Number(tokens[i + 1]);
    const kids = childRules(rule, type);
    if (!Number.isInteger(idx) || idx < 0 || idx >= kids.length) return {};
    rule = kids[idx];
    i += 2;
  }
  return { rule };
}

/* ---------------------------------------------------------- edits ---------- */

// Replace a scalar from a JSON value (the ergonomic set; create-or-replace).
export function setJson(text: string, pointer: string, jsonValue: string): string {
  const value = JSON.parse(jsonValue) as Json;
  const token = scalarToken(value); // throws on object/array/non-finite
  const root = postcss.parse(text);
  const tokens = splitPointer(pointer);
  if (tokens.length === 0) throw new Error("set: pointer must address a field");
  const key = tokens[tokens.length - 1];
  if (!isSafeName(key)) throw new Error("set: unsafe field key");
  const t = navigate(rootRule(root), tokens);
  if (t.decl) {
    t.decl.value = token;
  } else if (t.parent) {
    t.parent.append({ prop: "--" + key, value: token });
  } else {
    throw new Error("set: pointer did not resolve to a field");
  }
  return root.toString();
}

// Validate that `token` is exactly one CSSON scalar (no structural breakout):
// re-parse a probe and require one rule, one declaration, no nested rules.
function isSingleScalar(token: string): boolean {
  try {
    const probe = postcss.parse("x{--v:" + token + ";}");
    if (probe.nodes.length !== 1 || probe.first?.type !== "rule") return false;
    let decls = 0;
    let rules = 0;
    (probe.first as Rule).each((c) => {
      if (c.type === "decl") decls++;
      else if (c.type === "rule") rules++;
    });
    return decls === 1 && rules === 0;
  } catch {
    return false;
  }
}

// Replace a scalar at `pointer` from a raw CSSON token (advanced; prefer setJson).
export function set(text: string, pointer: string, token: string): string {
  if (!isSingleScalar(token)) throw new Error("set: value is not a single CSSON scalar");
  const root = postcss.parse(text);
  const tokens = splitPointer(pointer);
  if (tokens.length === 0) throw new Error("set: pointer must address a field");
  const t = navigate(rootRule(root), tokens);
  if (!t.decl) throw new Error("set: pointer must address a field");
  t.decl.value = token;
  return root.toString();
}

export function remove(text: string, pointer: string): string {
  const root = postcss.parse(text);
  const rr = rootRule(root);
  const tokens = splitPointer(pointer);
  if (tokens.length === 0) throw new Error("remove: cannot remove the document root");
  const t = navigate(rr, tokens);
  if (t.decl) t.decl.remove();
  else if (t.rule && t.rule !== rr) t.rule.remove();
  else throw new Error("remove: pointer did not resolve");
  return root.toString();
}

// Author a CSSON document from a JSON object (inverse of read).
function serializeBody(obj: { [k: string]: Json }, into: Rule): void {
  for (const key of Object.keys(obj)) {
    if (!isSafeName(key)) throw new Error("from-json: unsafe key");
    const v = obj[key];
    if (Array.isArray(v)) {
      for (const el of v) {
        if (!el || typeof el !== "object" || Array.isArray(el))
          throw new Error("array elements must be objects");
        const child = postcss.rule({ selector: key });
        serializeBody(el as any, child);
        into.append(child);
      }
    } else if (v && typeof v === "object") {
      const child = postcss.rule({ selector: key });
      serializeBody(v as any, child);
      into.append(child);
    } else {
      into.append({ prop: "--" + key, value: scalarToken(v) });
    }
  }
}

export function fromJson(jsonText: string): string {
  const obj = JSON.parse(jsonText);
  if (!obj || typeof obj !== "object" || Array.isArray(obj))
    throw new Error("from-json: the JSON root must be an object");
  const root = postcss.root();
  const rule = postcss.rule({ selector: "cssonv1" });
  serializeBody(obj, rule);
  root.append(rule);
  return root.toString();
}

/* --------------------------------------------------- RFC 6902 patch -------- */

function applyOp(root: Root, op: any): void {
  const tokens = splitPointer(op.path ?? "");
  if (op.op === "add" || op.op === "replace") {
    const v = op.value as Json;
    if (v && typeof v === "object") {
      // node insert/replace: serialize into the parent
      const key = tokens[tokens.length - 2];
      if (!isSafeName(key)) throw new Error("patch: unsafe node type");
      const parentTokens = tokens.slice(0, -2);
      const t = navigate(rootRule(root), parentTokens.length ? parentTokens : []);
      const parent = t.rule ?? rootRule(root);
      const els = Array.isArray(v) ? v : [v];
      for (const el of els) {
        const child = postcss.rule({ selector: key });
        serializeBody(el as any, child);
        parent.append(child);
      }
    } else {
      const key = tokens[tokens.length - 1];
      if (!isSafeName(key)) throw new Error("patch: unsafe field key");
      const t = navigate(rootRule(root), tokens);
      if (t.decl) t.decl.value = scalarToken(v);
      else if (t.parent) t.parent.append({ prop: "--" + key, value: scalarToken(v) });
      else throw new Error("patch: parent not found");
    }
  } else if (op.op === "remove") {
    const rr = rootRule(root);
    const t = navigate(rr, tokens);
    if (t.decl) t.decl.remove();
    else if (t.rule && t.rule !== rr) t.rule.remove();
    else throw new Error("patch: remove did not resolve");
  } else if (op.op === "test") {
    const got = pointerGet(JSON.parse(toCanonicalJson(root.toString())), tokens);
    if (JSON.stringify(got) !== JSON.stringify(op.value))
      throw new Error("patch: test failed");
  } else if (op.op === "move" || op.op === "copy") {
    const fromTokens = splitPointer(op.from ?? "");
    const val = pointerGet(JSON.parse(toCanonicalJson(root.toString())), fromTokens);
    if (val === undefined) throw new Error("patch: move/copy from not found");
    if (op.op === "move") applyOp(root, { op: "remove", path: op.from });
    applyOp(root, { op: "add", path: op.path, value: val });
  } else {
    throw new Error("patch: unknown op");
  }
}

export function patch(text: string, patchJson: string): string {
  const ops = JSON.parse(patchJson);
  if (!Array.isArray(ops)) throw new Error("patch must be a JSON array of operations");
  const root = postcss.parse(text); // atomic: all ops on one root; throw discards it
  for (const op of ops) {
    if (!op || typeof op.op !== "string" || typeof op.path !== "string")
      throw new Error("operation missing 'op' or 'path'");
    applyOp(root, op);
  }
  return root.toString();
}

/* ------------------------------------------------- CSSOM-style handle API -- */
// Rule handles address the raw CSS rule tree by child-rule INDEX PATH in source
// order (mirroring the C csson_rule, and ≈ a browser walking cssRules). The TS is
// stateless: each op re-parses the source the C sheet holds; edit ops return the
// new full source for the sheet to store, so comments/formatting survive.

function childNodesOf(rule: Rule): Rule[] {
  const out: Rule[] = [];
  rule.each((c: AnyNode) => {
    if (c.type === "rule") out.push(c);
  });
  return out;
}

function fieldDecls(rule: Rule): Declaration[] {
  const out: Declaration[] = [];
  rule.each((c: AnyNode) => {
    if (c.type === "decl" && c.prop.startsWith("--")) out.push(c as Declaration);
  });
  return out;
}

// Walk from the `cssonv1` root down child rules by index; returns root + node.
function resolvePath(text: string, pathJson: string): { root: Root; rule: Rule } {
  const root = postcss.parse(text);
  let rule = rootRule(root);
  for (const idx of JSON.parse(pathJson) as number[]) {
    const kids = childNodesOf(rule);
    if (!Number.isInteger(idx) || idx < 0 || idx >= kids.length)
      throw new Error("rule handle: index out of range");
    rule = kids[idx];
  }
  return { root, rule };
}

const propKey = (name: string): string => (name.startsWith("--") ? name.slice(2) : name);

export function hRuleCount(text: string, path: string): string {
  return String(childNodesOf(resolvePath(text, path).rule).length);
}
export function hSelector(text: string, path: string): string {
  return seltype(resolvePath(text, path).rule.selector);
}
export function hPropCount(text: string, path: string): string {
  return String(fieldDecls(resolvePath(text, path).rule).length);
}
export function hPropNameAt(text: string, path: string, iStr: string): string {
  const decls = fieldDecls(resolvePath(text, path).rule);
  const i = Number(iStr);
  if (!Number.isInteger(i) || i < 0 || i >= decls.length)
    throw new Error("property index out of range");
  return decls[i].prop; // e.g. "--org"
}
export function hGetProp(text: string, path: string, name: string): string {
  const d = findField(resolvePath(text, path).rule, propKey(name));
  if (!d) throw new Error("property absent"); // C maps the exception to NULL
  return d.value; // verbatim CSSON token
}
export function hGetPropValue(text: string, path: string, name: string): string {
  const d = findField(resolvePath(text, path).rule, propKey(name));
  if (!d) throw new Error("property absent");
  return JSON.stringify(coerce(d.value)); // coerced JSON scalar
}
export function hSetProp(text: string, path: string, name: string, jsonValue: string): string {
  const key = propKey(name);
  if (!isSafeName(key)) throw new Error("set: unsafe field key");
  const token = scalarToken(JSON.parse(jsonValue) as Json); // throws on object/array
  const { root, rule } = resolvePath(text, path);
  const d = findField(rule, key);
  if (d) d.value = token;
  else rule.append({ prop: "--" + key, value: token });
  return root.toString();
}
export function hRemoveProp(text: string, path: string, name: string): string {
  const { root, rule } = resolvePath(text, path);
  const d = findField(rule, propKey(name));
  if (!d) throw new Error("remove: property not found");
  d.remove();
  return root.toString();
}
export function hInsertRule(text: string, path: string, ruleText: string, idxStr: string): string {
  const { root, rule } = resolvePath(text, path);
  const frag = postcss.parse(ruleText);
  if (frag.nodes.length !== 1 || frag.first?.type !== "rule")
    throw new Error("insert: text must be exactly one CSSON node");
  const node = frag.first as Rule;
  if (!isSafeName(seltype(node.selector))) throw new Error("insert: unsafe node type");
  const kids = childNodesOf(rule);
  const idx = Number(idxStr);
  if (!Number.isInteger(idx) || idx < 0 || idx >= kids.length) rule.append(node); // -1 = append
  else kids[idx].before(node);
  return root.toString();
}
export function hDeleteRule(text: string, path: string, idxStr: string): string {
  const { root, rule } = resolvePath(text, path);
  const kids = childNodesOf(rule);
  const idx = Number(idxStr);
  if (!Number.isInteger(idx) || idx < 0 || idx >= kids.length)
    throw new Error("delete: index out of range");
  kids[idx].remove();
  return root.toString();
}

/* ------------------------------------------------------------ expose ------- */

(globalThis as any).csson = {
  toCanonicalJson,
  get,
  validate,
  set,
  setJson,
  remove,
  fromJson,
  patch,
  // CSSOM-style handle API (driven by the C csson_sheet / csson_rule layer)
  hRuleCount,
  hSelector,
  hPropCount,
  hPropNameAt,
  hGetProp,
  hGetPropValue,
  hSetProp,
  hRemoveProp,
  hInsertRule,
  hDeleteRule,
  version: "1",
};
