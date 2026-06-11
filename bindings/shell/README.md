# CSSON for Shell / CI

Status: **planned** (scaffold). Thin wrapper over the C core (`libcsson`, on liblexbor) in `../../core`.

| | |
|---|---|
| **Environment** | Shell / CI |
| **Front door** | CLI binary |
| **Mechanism** | invoke the `csson` binary; ship shell completions |
| **Distribution** | prebuilt binaries + Homebrew/apt/scoop |

## Contract
CSSON never writes its own parsing (see `../../CLAUDE.md`): this binding reads
via a real CSS parser for its environment and only walks the rule tree. It MUST
emit **byte-identical canonical JSON** for the shared suite in `../../conformance`.

## Build outline
1. Build the relevant front door in `../../core`
   (C ABI `libcsson` + `csson.h`, WASM, or the `csson` CLI).
2. Wrap it for Shell / CI via the mechanism above.
3. Run `../../conformance` fixtures in this environment's test harness.

## Versions & platforms
- **Supported CSSON standard versions:** 2 (selected via the core version arg;
  this binding forwards it and never branches on it).
- **Architectures:** amd64 + ARM64, distributed per `../../core/PLATFORMS.md`.
