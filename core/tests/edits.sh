#!/usr/bin/env bash
# Edit-path regression tests (run by ctest). $1 = path to the csson CLI.
# Guards the fixes for the edit/security bugs found in the code analysis.
set -uo pipefail
CS="${1:?usage: edits.sh <csson-binary>}"
tmp=$(mktemp -d "${PWD}/.edits.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
fail=0
chk() { # desc expected actual
  if [ "$2" = "$3" ]; then echo "ok: $1"; else echo "FAIL: $1"; echo "  exp: $2"; echo "  got: $3"; fail=1; fi
}

# H3 — removing a field that shares its line must NOT corrupt the document
printf 'cssonv1{ point { --x: 1; --y: 2; } }\n' > "$tmp/a-csson.css"
chk "H3 inline field rm" '{"point":[{"y":2}]}' "$("$CS" rm "$tmp/a-csson.css" /point/0/x | "$CS" canon -)"

# H4 — a brace inside a comment in the removed node must not fool the matcher
printf 'cssonv1{ a { /* } */ --x: 1; } b { --y: 2; } }\n' > "$tmp/b-csson.css"
chk "H4 comment-brace node rm" '{"b":[{"y":2}]}' "$("$CS" rm "$tmp/b-csson.css" /a/0 | "$CS" canon -)"

# H2 — a value that breaks out of the declaration must be rejected (injection)
printf 'cssonv1{ --x: 1; }\n' > "$tmp/c-csson.css"
"$CS" set "$tmp/c-csson.css" /x '2;} evil{--z:9' >/dev/null 2>&1
chk "H2 injection rejected" "1" "$?"
chk "H2 legit set" '{"x":42}' "$("$CS" set "$tmp/c-csson.css" /x '42' | "$CS" canon -)"

# M3 — removing the document root must be rejected
"$CS" rm "$tmp/c-csson.css" / >/dev/null 2>&1
chk "M3 root rm rejected" "1" "$?"

# Single-line node insert must land INSIDE the parent, not before it
printf 'cssonv1{ --x: 1; }\n' > "$tmp/ins-csson.css"
chk "single-line node insert" '{"dept":[{"name":"Eng"}],"x":1}' \
  "$("$CS" patch "$tmp/ins-csson.css" <(printf '[{"op":"add","path":"/dept/-","value":{"name":"Eng"}}]') | "$CS" canon -)"

# H1 — deeply nested input must error cleanly, not crash (no signal exit > 128)
python3 -c "open('$tmp/d-csson.css','w').write('cssonv1{'+'a{'*20000+'--x:1;'+'}'*20000+'}')"
"$CS" canon "$tmp/d-csson.css" >/dev/null 2>&1; rc=$?
if [ "$rc" -le 128 ]; then echo "ok: H1 deep no-crash (rc=$rc)"; else echo "FAIL: H1 deep crashed (rc=$rc)"; fail=1; fi

[ $fail = 0 ] && { echo "EDITS OK"; exit 0; } || { echo "EDITS FAILED"; exit 1; }
