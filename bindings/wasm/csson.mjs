// CSSON for WebAssembly — loads csson.wasm (QuickJS + PostCSS + csstree + our TS,
// the SAME engine as the native core) and exposes the C ABI as JS functions.
// Marshals UTF-8 strings across the wasm boundary and frees returned buffers.
//
// Runs anywhere with a WASI preview1 host: Node (node:wasi, below), Deno, or a
// browser via a wasi shim. The output is byte-identical to the native core.
import { WASI } from "node:wasi";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));

let ex; // wasm exports, initialised once
function engine() {
  if (ex) return ex;
  const wasi = new WASI({ version: "preview1", args: [], env: {} });
  const bytes = readFileSync(join(here, "csson.wasm"));
  const mod = new WebAssembly.Module(bytes);
  const inst = new WebAssembly.Instance(mod, wasi.getImportObject());
  wasi.initialize(inst); // reactor: runs _initialize
  ex = inst.exports;
  return ex;
}

const u8 = () => new Uint8Array(engine().memory.buffer);
const enc = new TextEncoder();
const dec = new TextDecoder();

// write bytes into wasm memory (NUL-terminated); returns [ptr, byteLength]
function put(str, nulTerminate = true) {
  const b = enc.encode(str);
  const ptr = engine().malloc(b.length + (nulTerminate ? 1 : 0));
  u8().set(b, ptr);
  if (nulTerminate) u8()[ptr + b.length] = 0;
  return [ptr, b.length];
}

function readCStr(ptr) {
  if (!ptr) return null;
  const mem = u8();
  let end = ptr;
  while (mem[end] !== 0) end++;
  return dec.decode(mem.subarray(ptr, end));
}

// Call a `(src,len,...args,err)` ABI function that returns an owned char*.
// args are NUL-terminated strings; throws the engine's *err message on failure.
function callStr(name, src, args) {
  const e = engine();
  const [srcPtr, srcLen] = put(src, false);
  const argPtrs = args.map((a) => put(a)[0]);
  const errPP = e.malloc(4);
  new DataView(e.memory.buffer).setUint32(errPP, 0, true);
  const ret = e[name](srcPtr, srcLen, ...argPtrs, errPP);
  let out, err;
  if (ret) {
    out = readCStr(ret);
    e.csson_free_string(ret);
  } else {
    const errPtr = new DataView(e.memory.buffer).getUint32(errPP, true);
    err = readCStr(errPtr);
    if (errPtr) e.csson_free_string(errPtr);
  }
  e.free(srcPtr);
  e.free(errPP);
  for (const p of argPtrs) e.free(p);
  if (ret) return out;
  throw new Error(err || "csson error");
}

export const toCanonicalJson = (text) => callStr("csson_to_canonical_json", text, []);
export const get = (text, pointer) => callStr("csson_get", text, [pointer]);
export const fromJson = (json) => callStr("csson_from_json", json, []);
export const set = (text, pointer, token) => callStr("csson_set", text, [pointer, token]);
export const setJson = (text, pointer, json) => callStr("csson_set_json", text, [pointer, json]);
export const remove = (text, pointer) => callStr("csson_remove", text, [pointer]);
export const patch = (text, patchJson) => callStr("csson_patch", text, [patchJson]);

export function validate(syntax, value) {
  // csson_validate takes (syntax, value, err) — syntax is arg0 here, not (src,len).
  const e = engine();
  const [sPtr] = put(syntax);
  const [vPtr] = put(value);
  const errPP = e.malloc(4);
  new DataView(e.memory.buffer).setUint32(errPP, 0, true);
  const r = e.csson_validate(sPtr, vPtr, errPP);
  e.free(sPtr);
  e.free(vPtr);
  e.free(errPP);
  if (r < 0) throw new Error("validate failed");
  return r === 1;
}

export const version = () => readCStr(engine().csson_supported_versions());

export default { toCanonicalJson, get, fromJson, set, setJson, remove, patch, validate, version };
