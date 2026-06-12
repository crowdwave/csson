import * as csson from "./csson.mjs";
const doc = `cssonv1 {
  --org: "Acme";
  dept { --name: "Eng"; --n: 3; }
}`;
console.log("version:", csson.version());
console.log("canon  :", csson.toCanonicalJson(doc));
console.log("get    :", csson.get(doc, "/dept/0/n"));
console.log("set    :", JSON.stringify(csson.setJson(doc, "/org", '"Beta"')).slice(0, 60));
console.log("valid  :", csson.validate("<integer>", "5"), csson.validate("<integer>", "5.5"));
