// CSSON for the browser (and any host without node:wasi). Loading wasm is async,
// so call `init()` once to get the API:
//
//   import { init } from "@csson/js/browser";
//   const csson = await init(new URL("./csson.wasm", import.meta.url));
//   csson.parse(text);
//
// In the browser, editing must run through this wasm core (the native CSSOM
// exposes no source offsets and editing via cssText strips comments) — and it is
// byte-identical to the native library, per the browser-parity invariant.
import { makeCsson, wasiShim, type Csson } from "./core.js";

type WasmSource = ArrayBuffer | Uint8Array | URL | string | Response;

async function bytesOf(src: WasmSource): Promise<ArrayBuffer> {
  if (src instanceof ArrayBuffer) return src;
  if (src instanceof Uint8Array) return src.buffer.slice(src.byteOffset, src.byteOffset + src.byteLength);
  if (typeof Response !== "undefined" && src instanceof Response) return src.arrayBuffer();
  return (await fetch(src as URL | string)).arrayBuffer();
}

/** Instantiate csson.wasm and return the CSSON API. */
export async function init(wasm: WasmSource): Promise<Csson> {
  const mem = { value: undefined as unknown as WebAssembly.Memory };
  const imports = { wasi_snapshot_preview1: wasiShim(() => mem.value) };
  const { instance } = await WebAssembly.instantiate(await bytesOf(wasm), imports);
  mem.value = instance.exports.memory as WebAssembly.Memory;
  const initialize = instance.exports._initialize as (() => void) | undefined;
  if (initialize) initialize(); // WASI reactor init
  return makeCsson(instance.exports as unknown as Parameters<typeof makeCsson>[0]);
}

export { CssonError } from "./core.js";
export type { Csson, Json } from "./core.js";
