// Conformance reader: canonical JSON via the CSSON WASM core (QuickJS+TS → wasm).
// Usage: node canon_wasm.js <file-csson.css>   (prints canonical JSON + newline)
import { readFileSync } from "node:fs";
import { toCanonicalJson } from "../../../../bindings/wasm/csson.mjs";
const file = process.argv[2];
process.stdout.write(toCanonicalJson(readFileSync(file, "utf8")) + "\n");
