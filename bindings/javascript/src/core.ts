// CSSON JS/TS binding — environment-agnostic core.
//
// Given an instantiated csson.wasm exports object, this builds the typed CSSON
// API and handles string marshaling across the wasm boundary (input via the
// exported malloc, returned buffers freed via csson_free_string). The same
// QuickJS + PostCSS + csstree core as the native library and browsers, so output
// is byte-identical across every CSSON engine.

export type Json = string | number | boolean | null | Json[] | { [k: string]: Json };

/** The wasm exports the core relies on (a subset of the C ABI + allocator). */
export interface CssonExports {
  memory: WebAssembly.Memory;
  malloc(n: number): number;
  free(p: number): void;
  csson_free_string(p: number): void;
  csson_supported_versions(): number;
  csson_to_canonical_json(src: number, len: number, err: number): number;
  csson_get(src: number, len: number, ptr: number, err: number): number;
  csson_from_json(src: number, len: number, err: number): number;
  csson_set(src: number, len: number, ptr: number, val: number, err: number): number;
  csson_set_json(src: number, len: number, ptr: number, val: number, err: number): number;
  csson_remove(src: number, len: number, ptr: number, err: number): number;
  csson_patch(src: number, len: number, patch: number, err: number): number;
  csson_validate(syntax: number, value: number, err: number): number;
}

/** The public CSSON API. */
export interface Csson {
  /** Parse a CSSON document to its canonical JSON value. */
  parse(text: string): Json;
  /** Read one value at an RFC 6901 JSON Pointer (""=whole doc). */
  get(text: string, pointer: string): Json;
  /** Replace the scalar at `pointer` from a JS value; returns new source (comments kept). */
  set(text: string, pointer: string, value: Json): string;
  /** Replace the scalar at `pointer` from a raw CSSON token (advanced). */
  setRaw(text: string, pointer: string, token: string): string;
  /** Remove the field/node at `pointer`; returns new source. */
  remove(text: string, pointer: string): string;
  /** Apply an RFC 6902 JSON Patch (atomic); returns new source. */
  patch(text: string, ops: Json[]): string;
  /** Serialize a JS object to a CSSON document (inverse of parse). */
  stringify(obj: Json): string;
  /** True if `value` matches a CSS @property `syntax` (e.g. "<integer>"). */
  validate(syntax: string, value: string): boolean;
  /** The supported CSSON standard version (e.g. "1"). */
  version(): string;
}

export class CssonError extends Error {}

const enc = new TextEncoder();
const dec = new TextDecoder();

export function makeCsson(ex: CssonExports): Csson {
  const u8 = () => new Uint8Array(ex.memory.buffer);
  const dv = () => new DataView(ex.memory.buffer);

  // write bytes into wasm memory (optionally NUL-terminated); returns [ptr, len]
  function put(str: string, nul = true): [number, number] {
    const b = enc.encode(str);
    const ptr = ex.malloc(b.length + (nul ? 1 : 0));
    u8().set(b, ptr);
    if (nul) u8()[ptr + b.length] = 0;
    return [ptr, b.length];
  }

  function readCStr(ptr: number): string {
    const mem = u8();
    let end = ptr;
    while (mem[end] !== 0) end++;
    return dec.decode(mem.subarray(ptr, end));
  }

  // Call a `(src,len,...args,err)` ABI fn returning an owned char*; throws on failure.
  function callStr(fn: (...a: number[]) => number, src: string, args: string[]): string {
    const [srcPtr, srcLen] = put(src, false);
    const argPtrs = args.map((a) => put(a)[0]);
    const errPP = ex.malloc(4);
    dv().setUint32(errPP, 0, true);
    const ret = fn(srcPtr, srcLen, ...argPtrs, errPP);
    let out: string | undefined;
    let err: string | undefined;
    if (ret) {
      out = readCStr(ret);
      ex.csson_free_string(ret);
    } else {
      const errPtr = dv().getUint32(errPP, true);
      err = errPtr ? readCStr(errPtr) : "csson error";
      if (errPtr) ex.csson_free_string(errPtr);
    }
    ex.free(srcPtr);
    ex.free(errPP);
    for (const p of argPtrs) ex.free(p);
    if (out !== undefined) return out;
    throw new CssonError(err);
  }

  return {
    parse: (text) => JSON.parse(callStr(ex.csson_to_canonical_json, text, [])),
    get: (text, pointer) => JSON.parse(callStr(ex.csson_get, text, [pointer])),
    set: (text, pointer, value) => callStr(ex.csson_set_json, text, [pointer, JSON.stringify(value)]),
    setRaw: (text, pointer, token) => callStr(ex.csson_set, text, [pointer, token]),
    remove: (text, pointer) => callStr(ex.csson_remove, text, [pointer]),
    patch: (text, ops) => callStr(ex.csson_patch, text, [JSON.stringify(ops)]),
    stringify: (obj) => callStr(ex.csson_from_json, JSON.stringify(obj), []),
    validate(syntax, value) {
      const [sPtr] = put(syntax);
      const [vPtr] = put(value);
      const errPP = ex.malloc(4);
      dv().setUint32(errPP, 0, true);
      const r = ex.csson_validate(sPtr, vPtr, errPP);
      ex.free(sPtr);
      ex.free(vPtr);
      ex.free(errPP);
      if (r < 0) throw new CssonError("validate failed");
      return r === 1;
    },
    version: () => readCStr(ex.csson_supported_versions()),
  };
}

/** Minimal WASI preview1 shim — csson.wasm only imports these five calls.
 *  Used in the browser (Node uses the built-in node:wasi instead). */
export function wasiShim(getMemory: () => WebAssembly.Memory) {
  const ok = 0;
  return {
    // monotonic/realtime clock -> write an i64 nanosecond timestamp
    clock_time_get(_id: number, _prec: bigint, out: number): number {
      const ns = BigInt(Math.round((typeof performance !== "undefined" ? performance.now() : Date.now()) * 1e6));
      new DataView(getMemory().buffer).setBigUint64(out, ns, true);
      return ok;
    },
    // stderr (fd 2) writes -> console; sum the iovec lengths into nwritten
    fd_write(fd: number, iovs: number, n: number, nwritten: number): number {
      const dv = new DataView(getMemory().buffer);
      const mem = new Uint8Array(getMemory().buffer);
      let total = 0;
      const parts: Uint8Array[] = [];
      for (let i = 0; i < n; i++) {
        const p = dv.getUint32(iovs + i * 8, true);
        const len = dv.getUint32(iovs + i * 8 + 4, true);
        parts.push(mem.subarray(p, p + len));
        total += len;
      }
      if (fd === 2 || fd === 1) {
        const msg = dec.decode(new Uint8Array(parts.flatMap((x) => [...x])));
        if (msg.trim()) (fd === 2 ? console.error : console.log)(msg);
      }
      dv.setUint32(nwritten, total, true);
      return ok;
    },
    fd_close: () => ok,
    fd_seek: () => ok,
    fd_fdstat_get: () => ok,
  };
}
