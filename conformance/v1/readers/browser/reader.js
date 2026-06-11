// CSSON browser reader — uses ONLY the browser's built-in CSSOM. No libraries.
//
// Walks the authored `cssRules` tree of a stylesheet and returns canonical JSON.
// Same algorithm as the lexbor core (core/src/csson_core.c): fields from
// `--custom` properties, children grouped by selector type (leading & stripped),
// keys sorted, compact, object arrays in source order. Reads the value via
// CSSOM `getPropertyValue` — the only built-in available; this is what the
// browser-parity invariant is tested against across engines.
//
// Injected verbatim into Chrome (CDP) and Firefox (WebDriver) by the drivers,
// and mirrored by packages/typescript/src/csson.ts for end users.
function cssonCanon(sheet) {
  var seltype = function (r) { return r.selectorText.replace(/^[&\s]+/, "").trim(); };
  var coerce = function (s) {
    s = s.trim();
    // canonical integer (no leading zeros / "-0") within the JS-safe range → number;
    // otherwise verbatim string. Matches the lexbor core exactly.
    if (/^(?:0|-?[1-9][0-9]*)$/.test(s)) {
      var num = Number(s);
      if (Number.isSafeInteger(num)) return num;
      return s;
    }
    if (s.length >= 2 && s[0] === '"' && s[s.length - 1] === '"') return s.slice(1, -1);
    return s;
  };
  var node = function (rule) {
    var o = {}, st = rule.style, i;
    for (i = 0; i < st.length; i++) {
      var p = st[i];
      if (p.indexOf("--") === 0) o[p.slice(2)] = coerce(st.getPropertyValue(p));
    }
    var kids = rule.cssRules || [];
    for (i = 0; i < kids.length; i++) {
      var r = kids[i];
      if (r.selectorText === undefined) continue;       // skip @property etc.
      var ty = seltype(r);
      (o[ty] = o[ty] || []).push(node(r));
    }
    return o;
  };
  var result = null, top = sheet.cssRules, k;
  for (k = 0; k < top.length; k++) {
    if (top[k].selectorText !== undefined && seltype(top[k]) === "cssonv1") result = node(top[k]);
  }
  var canon = function (v) {
    if (Array.isArray(v)) return v.map(canon);
    if (v && typeof v === "object") {
      var out = {}, keys = Object.keys(v).sort(), i;
      for (i = 0; i < keys.length; i++) out[keys[i]] = canon(v[keys[i]]);
      return out;
    }
    return v;
  };
  return JSON.stringify(canon(result));
}
