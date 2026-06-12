# preprocess.csson — CSS Syntax §3.3 input normalization

Asserts that all engines normalize the input stream identically before reading:

- `\r\n` (CRLF), a lone `\r` (CR), and `\f` (form feed, U+000C) all collapse to
  `\n`, so line-ending style never changes the data.

All three reference engines (C/QuickJS, Chrome/Blink, Firefox/Gecko) agree
byte-for-byte on the canonical JSON for this fixture.

## Known divergence: NUL (U+0000) in a value is NOT portable

CSS Syntax §3.3 says a U+0000 NULL in the input must be replaced with U+FFFD
(REPLACEMENT CHARACTER). Our core and Chrome do this — a value `p<NUL>q` reads
back with the U+FFFD replacement character between `p` and `q`. **Firefox does
not**: Gecko preserves the raw NUL, reading the same value with a literal U+0000.

Because two real browsers disagree, a NUL byte embedded in a CSSON value has no
cross-engine-stable meaning, so it is deliberately left OUT of the parity
fixture. Treat a literal NUL in a value as malformed input; do not rely on its
handling. (Our `preprocess()` follows the spec and replaces it with U+FFFD.)
