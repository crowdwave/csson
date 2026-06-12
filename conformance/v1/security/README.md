# CSSON security test set

Adversarial inputs that probe CSSON's attack surface (injection, parser
differentials, DoS). The S1–S9 findings these checks lock down are resolved on
the current core; this suite keeps them closed.

## Run
```sh
./run.sh                 # core/CLI checks (default ../../../core/build/csson)
CSSON=/path ./run.sh     # against a specific build
WITH_BROWSER=1 ./run.sh  # also run Chrome for the parser-differential (parity) checks
```

Each case prints one of:
- **PASS** — the attack is defended (secure behaviour).
- **VULNERABLE** — the attack succeeds; an OPEN finding (S1, S3, S5, …).
- **DIVERGENT** — the C core and the browser disagree on the same document; a
  parser-differential / byte-parity break (S2, S4).

The exit code is the number of OPEN findings still present, so once they are all
fixed `run.sh` exits 0 and can be wired into `ctest` as a security gate.

## What it covers
- **Baseline (must stay PASS):** scalar-value breakout via `set` (H2),
  document-root removal (M3), deep-nesting stack overflow (H1).
- **Injection:** S1 patch-API key/type/path injection, S3 `set` comment-injection.
- **Parser differentials / parity:** S2 duplicate custom property, S4 CSS §3.3
  input-preprocessing (CR/CRLF, NUL, invalid UTF-8).
- **DoS:** S5 O(N²) same-type-sibling amplification.
- **Conformance:** S7 RFC 6901 `~0`/`~1` decoding.

See the analysis doc for severity, CWE/OWASP mapping, CVE-class precedents, and
the recommended fix order.
