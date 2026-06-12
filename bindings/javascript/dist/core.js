// CSSON JS/TS binding — environment-agnostic core.
//
// Given an instantiated csson.wasm exports object, this builds the typed CSSON
// API and handles string marshaling across the wasm boundary (input via the
// exported malloc, returned buffers freed via csson_free_string). The same
// QuickJS + PostCSS + csstree core as the native library and browsers, so output
// is byte-identical across every CSSON engine.
export class CssonError extends Error {
}
const enc = new TextEncoder();
const dec = new TextDecoder();
export function makeCsson(ex) {
    const u8 = () => new Uint8Array(ex.memory.buffer);
    const dv = () => new DataView(ex.memory.buffer);
    // write bytes into wasm memory (optionally NUL-terminated); returns [ptr, len]
    function put(str, nul = true) {
        const b = enc.encode(str);
        const ptr = ex.malloc(b.length + (nul ? 1 : 0));
        u8().set(b, ptr);
        if (nul)
            u8()[ptr + b.length] = 0;
        return [ptr, b.length];
    }
    function readCStr(ptr) {
        const mem = u8();
        let end = ptr;
        while (mem[end] !== 0)
            end++;
        return dec.decode(mem.subarray(ptr, end));
    }
    // Call a `(src,len,...args,err)` ABI fn returning an owned char*; throws on failure.
    function callStr(fn, src, args) {
        const [srcPtr, srcLen] = put(src, false);
        const argPtrs = args.map((a) => put(a)[0]);
        const errPP = ex.malloc(4);
        dv().setUint32(errPP, 0, true);
        const ret = fn(srcPtr, srcLen, ...argPtrs, errPP);
        let out;
        let err;
        if (ret) {
            out = readCStr(ret);
            ex.csson_free_string(ret);
        }
        else {
            const errPtr = dv().getUint32(errPP, true);
            err = errPtr ? readCStr(errPtr) : "csson error";
            if (errPtr)
                ex.csson_free_string(errPtr);
        }
        ex.free(srcPtr);
        ex.free(errPP);
        for (const p of argPtrs)
            ex.free(p);
        if (out !== undefined)
            return out;
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
            if (r < 0)
                throw new CssonError("validate failed");
            return r === 1;
        },
        version: () => readCStr(ex.csson_supported_versions()),
    };
}
/** Minimal WASI preview1 shim — csson.wasm only imports these five calls.
 *  Used in the browser (Node uses the built-in node:wasi instead). */
export function wasiShim(getMemory) {
    const ok = 0;
    return {
        // monotonic/realtime clock -> write an i64 nanosecond timestamp
        clock_time_get(_id, _prec, out) {
            const ns = BigInt(Math.round((typeof performance !== "undefined" ? performance.now() : Date.now()) * 1e6));
            new DataView(getMemory().buffer).setBigUint64(out, ns, true);
            return ok;
        },
        // stderr (fd 2) writes -> console; sum the iovec lengths into nwritten
        fd_write(fd, iovs, n, nwritten) {
            const dv = new DataView(getMemory().buffer);
            const mem = new Uint8Array(getMemory().buffer);
            let total = 0;
            const parts = [];
            for (let i = 0; i < n; i++) {
                const p = dv.getUint32(iovs + i * 8, true);
                const len = dv.getUint32(iovs + i * 8 + 4, true);
                parts.push(mem.subarray(p, p + len));
                total += len;
            }
            if (fd === 2 || fd === 1) {
                const msg = dec.decode(new Uint8Array(parts.flatMap((x) => [...x])));
                if (msg.trim())
                    (fd === 2 ? console.error : console.log)(msg);
            }
            dv.setUint32(nwritten, total, true);
            return ok;
        },
        fd_close: () => ok,
        fd_seek: () => ok,
        fd_fdstat_get: () => ok,
    };
}
