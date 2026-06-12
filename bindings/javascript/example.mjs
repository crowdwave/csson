// Runnable tour of the CSSON JS/TS API (Node). From this directory:
//   npm run build && node example.mjs
import csson from "./dist/node.js";

const CONFIG = `/* deployment config — comments are first-class and survive edits */
cssonv1 {
  --environment: production;
  --port: 8080;
  --timeout: 30s;

  database {
    --engine: postgres;
    --pool-max: 10;
  }
  replica { --host: "db-1"; }
  replica { --host: "db-2"; }
}`;

console.log("CSSON version:", csson.version(), "\n");

// 1. Parse to a plain object (canonical JSON).
console.log("parsed:", csson.parse(CONFIG), "\n");

// 2. Read one value at a JSON Pointer.
console.log("pool-max:", csson.get(CONFIG, "/database/0/pool-max"));
console.log("replica[1].host:", csson.get(CONFIG, "/replica/1/host"), "\n");

// 3. Edit — comment-preserving, returns the new source text.
const updated = csson.set(CONFIG, "/port", 9090);
console.log("port now:", csson.parse(updated).port, "| comment kept:", updated.includes("/* deployment"), "\n");

// 4. RFC 6902 patch (atomic).
const patched = csson.patch(CONFIG, [
  { op: "replace", path: "/environment", value: "staging" },
  { op: "add", path: "/database/0/ssl", value: true },
]);
console.log("patched env:", csson.parse(patched).environment, "| ssl:", csson.parse(patched).database[0].ssl, "\n");

// 5. Build CSSON from a JS object.
console.log("stringify:\n" + csson.stringify({ service: { name: "api", replicas: 3 } }), "\n");

// 6. Validate against an @property syntax.
console.log("validate <integer> 5   ->", csson.validate("<integer>", "5"));
console.log("validate <integer> 5.5 ->", csson.validate("<integer>", "5.5"));
console.log("validate <color> red   ->", csson.validate("<color>", "red"));
