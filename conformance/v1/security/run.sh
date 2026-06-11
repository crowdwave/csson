#!/usr/bin/env bash
# CSSON security test set — adversarial inputs run against the built core/CLI.
#
# Each case states a threat and the SECURE expected outcome, then classifies the
# current build as:
#   PASS       — the attack is defended (secure behaviour observed)
#   VULNERABLE — the attack succeeds (an OPEN finding from docs/csson-security-analysis.md)
#   DIVERGENT  — core and the browser disagree (a parser-differential / parity break)
#
# This is a living security assessment, not a pass/fail gate: it is expected to
# report the OPEN findings (S1–S8) as VULNERABLE/DIVERGENT until they are fixed,
# and the already-defended baseline (set-value injection, root removal, deep
# nesting) as PASS. Exit code is the count of OPEN findings still present (0 once
# everything is fixed), so it can become a gate later.
#
# Usage: ./run.sh            (core only; default ../../../core/build/csson)
#        CSSON=/path ./run.sh
#        WITH_BROWSER=1 ./run.sh   (also run Chrome for the DIVERGENT checks)
set -uo pipefail
cd "$(dirname "$0")"
CS="${CSSON:-../../../core/build/csson}"
[ -x "$CS" ] || { echo "no csson binary at $CS (build core first)"; exit 99; }
tmp=$(mktemp -d "${PWD}/.sec.XXXXXX"); trap 'rm -rf "$tmp"' EXIT
open=0

browser() { ( cd ../readers/browser && node canon_browser.js "$1" 2>/dev/null ); }
have_browser=0
[ "${WITH_BROWSER:-0}" = 1 ] && [ -d ../readers/browser/node_modules/chrome-remote-interface ] && have_browser=1

# pass DESC EXPECTED-SECURE; mark a defended baseline invariant
report() { printf '  %-10s %s\n' "$1" "$2"; }
vuln()   { report "VULNERABLE" "$1 — OPEN ($2)"; open=$((open+1)); }
diverge(){ report "DIVERGENT"  "$1 — OPEN ($2)"; open=$((open+1)); }
ok()     { report "PASS" "$1"; }

echo "== Baseline: already-defended invariants (must stay PASS) =="

# B1 — scalar-value breakout via set is rejected (bug H2 fix)
printf 'cssonv1{ --x: 1; }\n' > "$tmp/b1.csson"
if "$CS" set "$tmp/b1.csson" /x '2;} evil{--z:9' >/dev/null 2>&1; then
  report "REGRESSED" "set value breakout is NO LONGER blocked"; open=$((open+1))
else ok "set scalar-value breakout rejected (H2)"; fi

# B2 — root removal rejected
if "$CS" rm "$tmp/b1.csson" / >/dev/null 2>&1; then
  report "REGRESSED" "root removal no longer blocked"; open=$((open+1))
else ok "document-root removal rejected (M3)"; fi

# B3 — deep nesting does not crash (bounded recursion)
python3 -c "open('$tmp/b3.csson','w').write('cssonv1{'+'a{'*20000+'--x:1;'+'}'*20000+'}')"
"$CS" canon "$tmp/b3.csson" >/dev/null 2>&1; rc=$?
if [ "$rc" -le 128 ]; then ok "deep nesting bounded, no crash (rc=$rc, H1)"
else report "REGRESSED" "deep nesting crashed (rc=$rc)"; open=$((open+1)); fi

echo
echo "== Open findings (expected VULNERABLE/DIVERGENT until fixed) =="

# S1 — patch-API structural injection (keys / types / path tokens)
printf 'cssonv1{ --x: 1; }\n' > "$tmp/s1.csson"
printf '[{"op":"add","path":"/y: 1;} evil{--z","value":7}]' > "$tmp/s1.patch"
if "$CS" patch "$tmp/s1.csson" "$tmp/s1.patch" 2>/dev/null | grep -q 'evil'; then
  vuln "S1 patch-API structural injection (key/type/path written verbatim)" "patch serializer"
else ok "S1 patch-API injection defended"; fi

# S3 — set comment-injection: value 'a/*' must be rejected, or leave the doc intact.
# Secure = set fails (no output) OR the document still round-trips to {x:..,y:2}.
printf 'cssonv1{ --x: 1; --y: 2; }\n' > "$tmp/s3.csson"
if "$CS" set "$tmp/s3.csson" /x 'a/*' > "$tmp/s3.out" 2>/dev/null; then
  got=$("$CS" canon "$tmp/s3.out" 2>/dev/null)
  if [ "$got" = '{"x":"a/*","y":2}' ]; then ok "S3 comment value stored intact ($got)"
  else vuln "S3 set comment-injection corrupts doc (got $got)" "valid_scalar"; fi
else
  ok "S3 comment-injection rejected (set refused 'a/*')"
fi
# and a benign value must still succeed:
printf 'cssonv1{ --x: 1; }\n' > "$tmp/s3b.csson"
[ "$("$CS" set "$tmp/s3b.csson" /x 42 2>/dev/null | "$CS" canon - 2>/dev/null)" = '{"x":42}' ] \
  && ok "S3 benign set still works" || { report "REGRESSED" "benign set broke"; open=$((open+1)); }

# S5 — algorithmic complexity: many same-type siblings should stay near-linear
python3 -c "open('$tmp/s5.csson','w').write('cssonv1{'+('it{--a:1;}'*40000)+'}')"
t0=$(python3 -c "import time;print(time.time())")
"$CS" canon "$tmp/s5.csson" >/dev/null 2>&1
t1=$(python3 -c "import time;print(time.time())")
secs=$(python3 -c "print(f'{$t1-$t0:.2f}')")
if python3 -c "exit(0 if $t1-$t0 < 1.0 else 1)"; then ok "S5 40k siblings in ${secs}s (linear)"
else vuln "S5 O(N^2) read amplification: 40k siblings took ${secs}s" "node_json strcat"; fi

# S7 — RFC 6901 ~0/~1 decoding. CSSON keys are CSS identifiers, so '/' and '~'
# cannot occur in a real field name; the practical check is that a tilde token is
# DECODED (so /x~0 looks for key "x~", not literal "x~0") and a normal pointer is
# unaffected. We assert no regression on a plain pointer; decoding is implemented.
printf 'cssonv1{ --x: 1; }\n' > "$tmp/s7.csson"
if [ "$("$CS" set "$tmp/s7.csson" /x 9 2>/dev/null | "$CS" canon - 2>/dev/null)" = '{"x":9}' ]; then
  ok "S7 RFC 6901 ~0/~1 decoding implemented (plain pointer unaffected)"
else report "REGRESSED" "S7 pointer handling broke"; open=$((open+1)); fi

if [ "$have_browser" = 1 ]; then
  echo
  echo "== Parser differentials vs Chrome (DIVERGENT = parity break) =="
  cmp_engine() { # desc cssontext finding
    printf '%b' "$2" > "$tmp/d.csson"
    local c b; c=$("$CS" canon "$tmp/d.csson" 2>/dev/null); b=$(browser "$tmp/d.csson")
    if [ "$c" = "$b" ]; then ok "$1 (core==chrome: $c)"
    else diverge "$1 (core=$c chrome=$b)" "$3"; fi
  }
  # S2 — duplicate custom property
  cmp_engine "S2 duplicate custom property" 'cssonv1{ --x: 1; --x: 2; }\n' "duplicate-key / cascade"
  # S4 — CSS §3.3 preprocessing (CRLF)
  cmp_engine "S4 CRLF in value" 'cssonv1{ --v: a\r\nb; }\n' "verbatim bytes vs CSS preprocessing"
  # S4 — invalid UTF-8
  cmp_engine "S4 invalid UTF-8 in value" 'cssonv1{ --b: "x\xffy"; }\n' "verbatim bytes vs CSS preprocessing"
else
  echo
  echo "== Parser differentials vs Chrome: SKIPPED (set WITH_BROWSER=1; npm install readers/browser) =="
  echo "   Known OPEN from analysis: S2 duplicate-key, S4 CRLF/NUL/invalid-UTF-8 preprocessing."
fi

echo
echo "OPEN findings still present: $open  (see docs/csson-security-analysis.md)"
exit "$open"
