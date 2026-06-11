#!/usr/bin/env bash
# Memory-safety regression: drive every libcsson code path — success AND the
# error/cleanup paths where leaks hide — and fail on any leak, invalid access or
# undefined behaviour. $1 = path to the csson CLI.
#
#   default              run bare; relies on the binary's own ASan/UBSan/LSan
#                        (this is how ctest runs it, against the sanitizer build)
#   MEMCHECK_VALGRIND=1  wrap each run in valgrind (use a NON-sanitized binary)
#
# Detects leaks/errors two ways: valgrind's exit code, or sanitizer text on stderr.
set -u
CS="${1:?usage: memcheck.sh <csson-binary>}"
cd "$(dirname "$0")"
EX="$PWD/../examples/sample_commented.csson"
FX="$PWD/../../conformance/v1/fixtures"
d=$(mktemp -d "$PWD/.mem.XXXXXX"); trap 'rm -rf "$d"' EXIT
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1}" UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1}"

VG=()
[ "${MEMCHECK_VALGRIND:-0}" = 1 ] && VG=(valgrind -q --leak-check=full
  --show-leak-kinds=definite,indirect --errors-for-leak-kinds=definite,indirect --error-exitcode=42)

pass=0; fail=0
run() { # description, then CLI args
  local desc="$1"; shift
  "${VG[@]}" "$CS" "$@" >/dev/null 2>"$d/e"
  local rc=$?
  if [ "$rc" -eq 42 ] || grep -qiE "AddressSanitizer|LeakSanitizer|runtime error|UndefinedBehavior" "$d/e"; then
    echo "FAIL (mem): $desc"; sed 's/^/    /' "$d/e" | head -10; fail=$((fail+1))
  else
    pass=$((pass+1))
  fi
}

# fixtures, including malformed / adversarial inputs
printf 'cssonv1 {\n  --org: "Acme";\n  dept { --name: "Eng"; --n: 3; }\n  dept { --name: "Ops"; }\n}\n' > "$d/doc.csson"
printf 'cssonv1{ --x: 1; --y: 2; }\n' > "$d/flat.csson"
printf 'cssonv1{ --x: 1; --x: 2; }\n' > "$d/dup.csson"
printf 'body{ --x: 1; }\n' > "$d/noroot.csson"
printf '\x00\xff\xfe garbage { not css ;;;; }}}}\n' > "$d/garbage.csson"
: > "$d/empty.csson"
python3 -c "open('$d/deep.csson','w').write('cssonv1{'+'a{'*20000+'--x:1;'+'}'*20000+'}')"

p() { printf '%s' "$2" > "$d/$1"; }
p addfield '[{"op":"add","path":"/dept/0/lead","value":"Alice"}]'
p replfield '[{"op":"add","path":"/dept/0/n","value":99}]'
p addnode  '[{"op":"add","path":"/team/-","value":{"name":"X"}}]'
p addarr   '[{"op":"add","path":"/team/-","value":[{"name":"A"},{"name":"B"}]}]'
p remove   '[{"op":"remove","path":"/dept/1"}]'
p replace  '[{"op":"replace","path":"/org","value":"Beta"}]'
p replnode '[{"op":"replace","path":"/dept/0","value":{"name":"Z"}}]'
p testok   '[{"op":"test","path":"/org","value":"Acme"}]'
p testbad  '[{"op":"test","path":"/org","value":"Nope"}]'
p move     '[{"op":"move","from":"/dept/0/n","path":"/dept/1/n"}]'
p copy     '[{"op":"copy","from":"/dept/0/name","path":"/dept/1/alias"}]'
p badjson  'not json at all'
p noop     '[{"path":"/x"}]'
p unknown  '[{"op":"frobnicate","path":"/x"}]'
p inject   '[{"op":"add","path":"/x }; evil{--z","value":7}]'
p injkey   '[{"op":"add","path":"/dept/0/k","value":{"a }; } evil{--z":1}}]'

# read path
run "canon doc"            canon "$d/doc.csson"
run "canon edge"           canon "$FX/edge.csson"
run "canon numeric"        canon "$FX/numeric.csson"
run "canon duplicate keys" canon "$d/dup.csson"
run "canon deep (err)"     canon "$d/deep.csson"
run "canon no-root (err)"  canon "$d/noroot.csson"
run "canon garbage (err)"  canon "$d/garbage.csson"
run "canon empty (err)"    canon "$d/empty.csson"
run "check valid"          check "$d/doc.csson"
run "check invalid"        check "$d/noroot.csson"
# set: success + every rejection/cleanup path
run "set valid"            set "$d/flat.csson" /x 42
run "set inject rejected"  set "$d/flat.csson" /x '2;} evil{--z:9'
run "set comment rejected" set "$d/flat.csson" /x 'a/*'
run "set not-found"        set "$d/flat.csson" /nope 1
run "set unresolved deep"  set "$d/flat.csson" /a/0/b 1
run "set root pointer"     set "$d/flat.csson" / 1
# remove
run "rm field"             rm "$d/flat.csson" /x
run "rm node"              rm "$d/doc.csson" /dept/1
run "rm root rejected"     rm "$d/doc.csson" /
[ -f "$EX" ] && run "rm node w/ comments" rm "$EX" /department/1
# patch: every op + error path
run "patch add field"      patch "$d/doc.csson" "$d/addfield"
run "patch replace field"  patch "$d/doc.csson" "$d/replfield"
run "patch add node"       patch "$d/doc.csson" "$d/addnode"
run "patch add node array" patch "$d/doc.csson" "$d/addarr"
run "patch add compact"    patch "$d/flat.csson" "$d/addnode"
run "patch remove"         patch "$d/doc.csson" "$d/remove"
run "patch replace2"       patch "$d/doc.csson" "$d/replace"
run "patch replace node"   patch "$d/doc.csson" "$d/replnode"
run "patch test ok"        patch "$d/doc.csson" "$d/testok"
run "patch test fail (err)" patch "$d/doc.csson" "$d/testbad"
run "patch move"           patch "$d/doc.csson" "$d/move"
run "patch copy"           patch "$d/doc.csson" "$d/copy"
run "patch bad json (err)" patch "$d/doc.csson" "$d/badjson"
run "patch no op (err)"    patch "$d/doc.csson" "$d/noop"
run "patch unknown (err)"  patch "$d/doc.csson" "$d/unknown"
run "patch inject path"    patch "$d/flat.csson" "$d/inject"
run "patch inject key"     patch "$d/doc.csson" "$d/injkey"
# CLI surface
run "versions"             versions
run "missing args"         set "$d/flat.csson"

echo "memcheck: $pass clean, $fail with leaks/errors${MEMCHECK_VALGRIND:+ (valgrind)}"
[ "$fail" -eq 0 ]
