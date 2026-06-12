// CSSON for Node.js — loads csson.wasm with the built-in node:wasi and exposes
// the API as ready-to-call functions (the engine loads lazily on first use, so
// `import { parse } from "@csson/js"; parse(text)` just works, synchronously).
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { WASI } from "node:wasi";
import { makeCsson } from "./core.js";
let api;
function engine() {
    if (api)
        return api;
    const here = dirname(fileURLToPath(import.meta.url));
    const wasmPath = process.env.CSSON_WASM ||
        [join(here, "csson.wasm"), join(here, "..", "csson.wasm")].find((p) => {
            try {
                readFileSync(p);
                return true;
            }
            catch {
                return false;
            }
        }) ||
        join(here, "..", "csson.wasm");
    const wasi = new WASI({ version: "preview1", args: [], env: {} });
    const mod = new WebAssembly.Module(readFileSync(wasmPath));
    const inst = new WebAssembly.Instance(mod, wasi.getImportObject());
    wasi.initialize(inst);
    api = makeCsson(inst.exports);
    return api;
}
export const parse = (text) => engine().parse(text);
export const get = (text, pointer) => engine().get(text, pointer);
export const set = (text, pointer, value) => engine().set(text, pointer, value);
export const setRaw = (text, pointer, token) => engine().setRaw(text, pointer, token);
export const remove = (text, pointer) => engine().remove(text, pointer);
export const patch = (text, ops) => engine().patch(text, ops);
export const stringify = (obj) => engine().stringify(obj);
export const validate = (syntax, value) => engine().validate(syntax, value);
export const version = () => engine().version();
export { CssonError } from "./core.js";
export default { parse, get, set, setRaw, remove, patch, stringify, validate, version };
