import assert from "node:assert";
import csson, { parse, get, set, remove, patch, stringify, validate, version, CssonError } from "./dist/node.js";

const DOC = `cssonv1 {
  --org: "Acme";   /* keep me */
  dept { --name: "Eng"; --n: 3; }
  dept { --name: "Ops"; }
}`;

let n = 0; const ok = (m) => { console.log("  ok  " + m); n++; };
assert.equal(version(), "1"); ok("version");
assert.deepEqual(parse(DOC), { org: "Acme", dept: [{ n: 3, name: "Eng" }, { name: "Ops" }] }); ok("parse");
assert.equal(get(DOC, "/dept/0/n"), 3); ok("get");
const e = set(DOC, "/org", "Beta");
assert.ok(e.includes("/* keep me */") && parse(e).org === "Beta"); ok("set (comment preserved)");
assert.ok(!("org" in parse(remove(DOC, "/org")))); ok("remove");
assert.equal(parse(patch(DOC, [{ op: "replace", path: "/org", value: "Z" }])).org, "Z"); ok("patch");
assert.deepEqual(parse(stringify({ app: { port: 8080 } })), { app: [{ port: 8080 }] }); ok("stringify");
assert.equal(validate("<integer>", "5"), true);
assert.equal(validate("<integer>", "5.5"), false); ok("validate");
assert.throws(() => parse("body{--x:1;}"), CssonError); ok("error on no root");
assert.equal(csson.version(), "1"); ok("default export");
console.log(`\n${n} passed`);
