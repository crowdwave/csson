#!/usr/bin/env bash
# Build the CSSON core (facade.c + QuickJS-ng + the TS bundle) to a WebAssembly
# reactor module exporting the C ABI (csson.h). Same engine + same TypeScript as
# the native core, so the WASM reader is byte-identical (browser-parity invariant).
#
# Requires wasi-sdk (clang + wasm32-wasi sysroot). Override with WASI_SDK=...
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
core="$here/../../core"
out="$here/build"
mkdir -p "$out"

WASI_SDK="${WASI_SDK:-$here/../../tmp/wasi-sdk}"
CLANG="$WASI_SDK/bin/clang"
[ -x "$CLANG" ] || { echo "wasi-sdk clang not found at $CLANG (set WASI_SDK=...)"; exit 1; }

# QuickJS sources: reuse the pinned clone from the native build, else clone it.
QJS="${QJS_DIR:-$core/build/quickjs}"
if [ ! -f "$QJS/quickjs.c" ]; then
  QJS="$out/quickjs"
  [ -f "$QJS/quickjs.c" ] || git clone --depth 1 --branch v0.15.1 \
    https://github.com/quickjs-ng/quickjs.git "$QJS"
fi

# The JS bundle (our TS + PostCSS + csstree) as a C byte array.
echo "[1/4] bundling TypeScript…"
( cd "$core" && [ -d node_modules ] || npm install --silent )
node "$core/tools/build-bundle.mjs" "$out/csson_bundle.h"

WASI_DEFS=(-D_WASI_EMULATED_PROCESS_CLOCKS -D_WASI_EMULATED_SIGNAL)

echo "[2/4] compiling QuickJS (wasm32-wasi)…"
for f in quickjs libregexp libunicode dtoa; do
  "$CLANG" -Os -std=gnu11 -DCONFIG_VERSION='"csson"' "${WASI_DEFS[@]}" -w \
    -I"$QJS" -c "$QJS/$f.c" -o "$out/$f.o"
done

echo "[3/4] compiling the CSSON facade (C23)…"
"$CLANG" -Os -std=c23 "${WASI_DEFS[@]}" \
  -I"$QJS" -I"$out" -I"$core/include" \
  -c "$core/src/facade.c" -o "$out/facade.o"

echo "[4/4] linking reactor → csson.wasm…"
# Exported ABI: the stateless document API + the CSSOM handle API + malloc/free
# (so the JS host can place input bytes and release returned strings).
EXPORTS=(
  csson_to_canonical_json csson_get csson_from_json csson_validate
  csson_set csson_set_json csson_remove csson_patch
  csson_supported_versions csson_free_string csson_error_kind_of
  csson_open csson_sheet_text csson_close csson_root
  csson_rule_count csson_rule_at csson_selector_text
  csson_property_count csson_property_name_at
  csson_get_property csson_get_property_value
  csson_set_property csson_remove_property
  csson_insert_rule csson_delete_rule
  malloc free
)
link_exports=()
for e in "${EXPORTS[@]}"; do link_exports+=("-Wl,--export=$e"); done

"$CLANG" -Os -mexec-model=reactor "$out"/*.o \
  -lwasi-emulated-process-clocks -lwasi-emulated-signal \
  "${link_exports[@]}" \
  -Wl,-z,stack-size=8388608 \
  -Wl,--initial-memory=16777216 -Wl,--max-memory=2147483648 \
  -Wl,--no-entry \
  -o "$here/csson.wasm"

echo "done: $here/csson.wasm ($(du -h "$here/csson.wasm" | cut -f1))"
