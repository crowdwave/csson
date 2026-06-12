import { type Csson } from "./core.js";
type WasmSource = ArrayBuffer | Uint8Array | URL | string | Response;
/** Instantiate csson.wasm and return the CSSON API. */
export declare function init(wasm: WasmSource): Promise<Csson>;
export { CssonError } from "./core.js";
export type { Csson, Json } from "./core.js";
