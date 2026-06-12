[← all docs](../README.md)

# Building CSSON from source

You do **not** need to build anything to *use* CSSON — every binding ships a
prebuilt artifact (see the [implementation guides](../README.md#implementations)).
Build from source only to develop CSSON, target a platform the prebuilt binaries
don't cover, or run the test suites.

Every command below is copy-paste runnable from the repository root and lists the
output you should see.

## Prerequisites

| Tool | Version | Needed for |
|---|---|---|
| **git** | any | cloning the source (CMake also clones the pinned QuickJS engine) |
| **CMake** | ≥ 3.28 | the core build |
| **C compiler** | GCC ≥ 14 or Clang ≥ 18 | C23 (the facade) |
| **Node.js + npm** | Node ≥ 18 | builds the embedded JS bundle (PostCSS + csstree + the TS) |
| wasi-sdk | ≥ 22 | *optional* — only to rebuild `csson.wasm` |
| valgrind | any | *optional* — the `memcheck` test |

Check them:

```sh
cmake --version     # cmake version 3.28 or newer
cc --version        # gcc 14+ / clang 18+
node --version      # v18 or newer
```

## 1. Get the source

```sh
git clone https://github.com/crowdwave/csson.git
cd csson
```

## 2. Build the core (C library + CLI)

```sh
cmake -S core -B core/build -DCMAKE_BUILD_TYPE=Release
cmake --build core/build -j
```

The first `configure` clones the pinned **quickjs-ng** into `core/build/` and runs
`npm install` in `core/` to build the JS bundle — so this step needs network and
Node the first time. It produces:

- `core/build/csson` — the CLI
- `core/build/libcsson.a` — the static library (C ABI in `core/include/csson.h`)
- `core/build/libcsson.so` — the **shared** library used by the Python binding

**Verify it built:**

```sh
./core/build/csson --version
# csson 0.2.0 (CSSON standard v1)

echo 'cssonv1{ --x: 1; }' | ./core/build/csson canon -
# {"x":1}
```

## 3. Run the tests

```sh
ctest --test-dir core/build
# 100% tests passed, 0 tests failed out of 9
```

The 9 tests cover canonical JSON against the conformance fixtures, comment-preserving
edits, the handle API, the S1–S9 security suite, and `memcheck`.

### Sanitizers (optional)

```sh
cmake -S core -B core/build-san -DCMAKE_BUILD_TYPE=Debug -DCSSON_SANITIZE=ON
cmake --build core/build-san -j
ctest --test-dir core/build-san        # runs memcheck under ASan/UBSan/LSan
```

## 4. Rebuild the WebAssembly core (optional)

`bindings/wasm/csson.wasm` is committed prebuilt; rebuild it only if you change the
core. You need **wasi-sdk** (clang + a `wasm32-wasi` sysroot):

```sh
# download wasi-sdk for your OS from https://github.com/WebAssembly/wasi-sdk/releases
# (example: Linux x86-64, wasi-sdk 33)
curl -fsSL -o /tmp/wasi-sdk.tar.gz \
  https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-33/wasi-sdk-33.0-x86_64-linux.tar.gz
mkdir -p tmp && tar -xzf /tmp/wasi-sdk.tar.gz -C tmp && mv tmp/wasi-sdk-33.0-x86_64-linux tmp/wasi-sdk

bash bindings/wasm/build.sh
# done: bindings/wasm/csson.wasm (~1.4 MB)
```

The script auto-discovers `tmp/wasi-sdk`, or set `WASI_SDK=/path/to/wasi-sdk`.

## 5. Rebuild the JavaScript binding (optional)

`bindings/javascript/dist/` and its `csson.wasm` are committed prebuilt; rebuild
only if you change the TypeScript:

```sh
cd bindings/javascript
npm install
npm run build       # compiles src/*.ts -> dist/ and copies in csson.wasm
cd ../..
```

## 6. Cross-engine conformance (optional)

Prove the canonical JSON is byte-identical across engines:

```sh
cd conformance/v1
CSSON=../../core/build/csson bash verify.sh
# RESULT: all checked readers match expected
cd ../..
```

The C core and the WASM reader run with no extra setup; the Chrome (Blink) and
Firefox (Gecko) readers are skipped unless their drivers are installed (see
`conformance/v1/readers/*/`).

---

That's the whole build. For day-to-day *use*, go back to the
[implementation guides](../README.md#implementations) — none of them require any
of the above.
