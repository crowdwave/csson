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
EX="$PWD/../examples/sample_commented-csson.css"
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
printf 'cssonv1 {\n  --org: "Acme";\n  dept { --name: "Eng"; --n: 3; }\n  dept { --name: "Ops"; }\n}\n' > "$d/doc-csson.css"
printf 'cssonv1{ --x: 1; --y: 2; }\n' > "$d/flat-csson.css"
printf 'cssonv1{ --x: 1; --x: 2; }\n' > "$d/dup-csson.css"
printf 'body{ --x: 1; }\n' > "$d/noroot-csson.css"
printf '\x00\xff\xfe garbage { not css ;;;; }}}}\n' > "$d/garbage-csson.css"
: > "$d/empty-csson.css"
python3 -c "open('$d/deep-csson.css','w').write('cssonv1{'+'a{'*20000+'--x:1;'+'}'*20000+'}')"

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
p fj       '{"org":"Acme","dept":[{"name":"Eng","size":12},{"name":"Ops"}]}'
p fjbad    'not json'
p fjarr    '[1,2,3]'

# read path
run "canon doc"            canon "$d/doc-csson.css"
run "canon edge"           canon "$FX/edge-csson.css"
run "canon numeric"        canon "$FX/numeric-csson.css"
run "canon duplicate keys" canon "$d/dup-csson.css"
run "canon deep (err)"     canon "$d/deep-csson.css"
run "canon no-root (err)"  canon "$d/noroot-csson.css"
run "canon garbage (err)"  canon "$d/garbage-csson.css"
run "canon empty (err)"    canon "$d/empty-csson.css"
run "check valid"          check "$d/doc-csson.css"
run "check invalid"        check "$d/noroot-csson.css"
# set: success + every rejection/cleanup path
run "set valid"            set "$d/flat-csson.css" /x 42
run "set inject rejected"  set "$d/flat-csson.css" /x '2;} evil{--z:9'
run "set comment rejected" set "$d/flat-csson.css" /x 'a/*'
run "set not-found"        set "$d/flat-csson.css" /nope 1
run "set unresolved deep"  set "$d/flat-csson.css" /a/0/b 1
run "set root pointer"     set "$d/flat-csson.css" / 1
# remove
run "rm field"             rm "$d/flat-csson.css" /x
run "rm node"              rm "$d/doc-csson.css" /dept/1
run "rm root rejected"     rm "$d/doc-csson.css" /
[ -f "$EX" ] && run "rm node w/ comments" rm "$EX" /department/1
# patch: every op + error path
run "patch add field"      patch "$d/doc-csson.css" "$d/addfield"
run "patch replace field"  patch "$d/doc-csson.css" "$d/replfield"
run "patch add node"       patch "$d/doc-csson.css" "$d/addnode"
run "patch add node array" patch "$d/doc-csson.css" "$d/addarr"
run "patch add compact"    patch "$d/flat-csson.css" "$d/addnode"
run "patch remove"         patch "$d/doc-csson.css" "$d/remove"
run "patch replace2"       patch "$d/doc-csson.css" "$d/replace"
run "patch replace node"   patch "$d/doc-csson.css" "$d/replnode"
run "patch test ok"        patch "$d/doc-csson.css" "$d/testok"
run "patch test fail (err)" patch "$d/doc-csson.css" "$d/testbad"
run "patch move"           patch "$d/doc-csson.css" "$d/move"
run "patch copy"           patch "$d/doc-csson.css" "$d/copy"
run "patch bad json (err)" patch "$d/doc-csson.css" "$d/badjson"
run "patch no op (err)"    patch "$d/doc-csson.css" "$d/noop"
run "patch unknown (err)"  patch "$d/doc-csson.css" "$d/unknown"
run "patch inject path"    patch "$d/flat-csson.css" "$d/inject"
run "patch inject key"     patch "$d/doc-csson.css" "$d/injkey"
# JSON-bridge API
run "get scalar"           get "$d/doc-csson.css" /org
run "get subobject"        get "$d/doc-csson.css" /dept/0
run "get not-found (err)"  get "$d/doc-csson.css" /nope
run "get root"             get "$d/doc-csson.css" ""
run "from-json valid"      from-json "$d/fj"
run "from-json bad (err)"  from-json "$d/fjbad"
run "from-json array (err)" from-json "$d/fjarr"
run "set-json scalar"      set-json "$d/flat-csson.css" /x '"hi"'
run "set-json object (err)" set-json "$d/flat-csson.css" /x '{"a":1}'
# validate (@property syntax checking — no file)
run "validate match"       validate "<integer>" 5
run "validate no-match"    validate "<integer>" 5.5
run "validate color"       validate "<color>" red
run "validate universal"   validate "*" "anything here"
run "validate bad syntax"  validate "<nonsense" x
# CLI surface
run "versions"             versions
run "missing args"         set "$d/flat-csson.css"

# fuzz sweep: seeded, mutated/malformed inputs must never crash or trip a
# sanitizer/valgrind — only ever a clean error exit. Deterministic (fixed seed).
FUZZ_N="${FUZZ_N:-200}"
python3 - "$d" "$FUZZ_N" <<'PY'
import sys, os, random
d, n = sys.argv[1], int(sys.argv[2])
random.seed(1234)
seeds = [
  b'cssonv1{ --x: 1; dept { --name: "Eng"; --n: 3; } }',
  b'cssonv1{ --a: 1, 2, 3; --b: 10px 20px; --t: 30s; }',
  b'@property --x { syntax: "<integer>"; inherits: false; }',
  b'cssonv1{ --u: url("http://x"); --c: #fff; --k: oklch(0.6 0.1 20); }',
  b'body { color: red }',
]
inj = [b'{', b'}', b';', b'"', b'/*', b'*/', b'\\', b'\x00', b'cssonv1', b'--', b'(', b')', b':']
os.makedirs(d+'/fuzz', exist_ok=True)
for i in range(n):
    b = bytearray(random.choice(seeds))
    for _ in range(random.randint(1, 8)):
        if not b: b += b'cssonv1{}'
        op, p = random.random(), random.randrange(len(b))
        if op < 0.4:                    b[p] ^= 1 << random.randrange(8)  # bit flip
        elif op < 0.7:                  b[p:p] = random.choice(inj)       # inject
        elif op < 0.85 and len(b) > 1:  del b[p]                          # delete
        else:                           b += random.choice(inj)          # append
    open(f'{d}/fuzz/{i}-csson.css', 'wb').write(b)
PY
fz=0
for f in "$d"/fuzz/*-csson.css; do
  "${VG[@]}" "$CS" canon "$f" >/dev/null 2>"$d/e"; rc=$?
  if [ "$rc" -ge 128 ] || [ "$rc" -eq 42 ] || \
     grep -qiE "AddressSanitizer|LeakSanitizer|runtime error|UndefinedBehavior" "$d/e"; then
    echo "FAIL (fuzz): $f rc=$rc"; sed 's/^/    /' "$d/e" | head -8; fz=$((fz+1))
  fi
done
[ "$fz" -eq 0 ] && pass=$((pass+1)) || fail=$((fail+1))
echo "fuzz: $FUZZ_N inputs, $fz crashes/errors"

echo "memcheck: $pass clean, $fail with leaks/errors${MEMCHECK_VALGRIND:+ (valgrind)}"
[ "$fail" -eq 0 ]
