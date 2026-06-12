// CSSON for Node.js — loads csson.wasm with the built-in node:wasi and exposes
// the API as ready-to-call functions (the engine loads lazily on first use, so
// `import { parse } from "@csson/js"; parse(text)` just works, synchronously).
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { WASI } from "node:wasi";
import { makeCsson, type Csson, type Json } from "./core.js";

let api: Csson | undefined;

function engine(): Csson {
  if (api) return api;
  const here = dirname(fileURLToPath(import.meta.url));
  const wasmPath =
    process.env.CSSON_WASM ||
    [join(here, "csson.wasm"), join(here, "..", "csson.wasm")].find((p) => {
      try { readFileSync(p); return true; } catch { return false; }
    }) ||
    join(here, "..", "csson.wasm");
  const wasi = new WASI({ version: "preview1", args: [], env: {} });
  const mod = new WebAssembly.Module(readFileSync(wasmPath));
  const inst = new WebAssembly.Instance(mod, wasi.getImportObject() as WebAssembly.Imports);
  wasi.initialize(inst);
  api = makeCsson(inst.exports as unknown as Parameters<typeof makeCsson>[0]);
  return api;
}

export const parse = (text: string): Json => engine().parse(text);
export const get = (text: string, pointer: string): Json => engine().get(text, pointer);
export const set = (text: string, pointer: string, value: Json): string => engine().set(text, pointer, value);
export const setRaw = (text: string, pointer: string, token: string): string => engine().setRaw(text, pointer, token);
export const remove = (text: string, pointer: string): string => engine().remove(text, pointer);
export const patch = (text: string, ops: Json[]): string => engine().patch(text, ops);
export const stringify = (obj: Json): string => engine().stringify(obj);
export const validate = (syntax: string, value: string): boolean => engine().validate(syntax, value);
export const version = (): string => engine().version();

export { CssonError } from "./core.js";
export type { Csson, Json } from "./core.js";
export default { parse, get, set, setRaw, remove, patch, stringify, validate, version };
