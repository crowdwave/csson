# CSSON for Java / JVM

Status: **planned** (scaffold). Thin wrapper over the C core (`libcsson`, on liblexbor) in `../../core`.

| | |
|---|---|
| **Environment** | Java / JVM |
| **Front door** | C ABI |
| **Mechanism** | JNI or Project Panama (FFM) over libcsson |
| **Distribution** | Maven Central |

## Contract
CSSON never writes its own parsing (see `../../CLAUDE.md`): this binding reads
via a real CSS parser for its environment and only walks the rule tree. It MUST
emit **byte-identical canonical JSON** for the shared suite in `../../conformance`.

## Build outline
1. Build the relevant front door in `../../core`
   (C ABI `libcsson` + `csson.h`, WASM, or the `csson` CLI).
2. Wrap it for Java / JVM via the mechanism above.
3. Run `../../conformance` fixtures in this environment's test harness.

## Versions & platforms
- **Supported CSSON standard versions:** 2 (selected via the core version arg;
  this binding forwards it and never branches on it).
- **Architectures:** amd64 + ARM64, distributed per `../../core/PLATFORMS.md`.
