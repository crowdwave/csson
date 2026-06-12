export type Json = string | number | boolean | null | Json[] | {
    [k: string]: Json;
};
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
export declare class CssonError extends Error {
}
export declare function makeCsson(ex: CssonExports): Csson;
/** Minimal WASI preview1 shim — csson.wasm only imports these five calls.
 *  Used in the browser (Node uses the built-in node:wasi instead). */
export declare function wasiShim(getMemory: () => WebAssembly.Memory): {
    clock_time_get(_id: number, _prec: bigint, out: number): number;
    fd_write(fd: number, iovs: number, n: number, nwritten: number): number;
    fd_close: () => number;
    fd_seek: () => number;
    fd_fdstat_get: () => number;
};
