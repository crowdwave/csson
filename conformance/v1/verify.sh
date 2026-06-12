#!/usr/bin/env bash
# Cross-engine conformance: every built engine must emit byte-identical canonical
# JSON for each fixture. Browser parity is the non-negotiable invariant
# (see ../../CLAUDE.md): the same output must come from the lexbor core, Chrome
# (Blink) and Firefox (Gecko).
set -uo pipefail
cd "$(dirname "$0")"
out="$PWD/.verify-out"; mkdir -p "$out"; fail=0

CORE="${CSSON:-../../core/build/csson}"           # the lexbor C engine = the libcsson CLI
have_c=0;  [ -x "$CORE" ] && have_c=1
have_js=0; [ -d readers/browser/node_modules/chrome-remote-interface ] && have_js=1
have_ff=0; { [ -x "${GECKODRIVER:-../../tmp/tools/geckodriver}" ] && [ -x "${FIREFOX_BIN:-../../tmp/tools/firefox/firefox}" ]; } && have_ff=1
have_wasm=0; [ -f ../../bindings/wasm/csson.wasm ] && have_wasm=1

run_doc() {  # $1=doc  $2=expected  $3=engine list
  local doc="$PWD/$1" exp="$2" engines="$3" name; name=$(basename "$1")
  echo "== $name =="
  for eng in $engines; do
    case $eng in
      c)       [ $have_c  = 1 ] || { echo "  core(c): SKIP (build core: cmake --build core/build)"; continue; }; "$CORE" canon "$doc" > "$out/$name.c.json" ;;
      browser) [ $have_js  = 1 ] || { echo "  chrome: SKIP (npm install readers/browser)"; continue; }; ( cd readers/browser && node canon_browser.js "$doc" > "$out/$name.browser.json" ) ;;
      firefox) [ $have_ff  = 1 ] || { echo "  firefox: SKIP (run readers/firefox/setup.sh)"; continue; }; node readers/firefox/canon_firefox.js "$doc" > "$out/$name.firefox.json" ;;
      wasm)    [ $have_wasm = 1 ] || { echo "  wasm: SKIP (build: bindings/wasm/build.sh)"; continue; }; node readers/wasm/canon_wasm.js "$doc" > "$out/$name.wasm.json" ;;
    esac
    if diff -q "$out/$name.$eng.json" "$exp" >/dev/null; then echo "  $eng: identical"; else echo "  $eng: MISMATCH"; fail=1; fi
  done
}

run_doc csson_v1.csson            expected.json                     "c browser firefox wasm"
run_doc fixtures/numeric.csson    fixtures/numeric.expected.json    "c browser firefox wasm"
run_doc fixtures/edge.csson       fixtures/edge.expected.json       "c browser firefox wasm"
run_doc fixtures/preprocess.csson fixtures/preprocess.expected.json "c browser firefox wasm"

echo
[ $fail = 0 ] && echo "RESULT: all checked readers match expected" || echo "RESULT: mismatch (see above)"
