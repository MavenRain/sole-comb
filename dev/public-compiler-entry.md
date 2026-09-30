# Public compiler entry decision

Adopted on 2026-09-29 from the user's instruction: "We need .sole-comb examples,
and we need to be working on the real, public compiler ASAP."

The public compiler and native source examples take immediate priority.
This supersedes the M0 plan's scheduling of the native lexer only at C1.
The supported native frontend and public driver may land alongside Stage A.
The original complete Stage A differential, semantic changes, and later
backend gates remain required for their respective acceptance claims.

The first increment replaces the constant-returning host entry with a driver
that parses and checks exact source bytes. The tracked `./sole-comb` command
provides `check FILE.sole-comb`, optional checked kernel output, and explicit
host overrides. Bun is the development default until the qualified endpoint
is selected. The decision does not change R2 measurements or select an endpoint.

Production modules reuse the validated pinned parser and elaborator port under
`surface/`, with native lexical admission, braces and semicolon-separated
`elim` arms. Production imports still cannot reach `test/pinfront/`; the pinned
oracle frontend remains independent. `surface/source.bend` rejects the deleted
legacy forms. The finite-sum eliminator lowers to the existing checked kernel
Elim. Each supported arm is a single-binder function, and its annotation is
checked against the corresponding payload type. Motives are supported.

The supported prefix includes ordinary definitions, axioms, dependent
functions, Nat primitives, sums, products, and finite-sum elimination.
Recursive-family recursors, constructor-labelled arms, arbitrary arm terms,
records, default arms, type-directed erasure, and WebAssembly build/run remain
pending. Unsupported forms must fail, and native examples for them must enter
the public suite when they become operational. The first increment's source
transport accepts files up to 32768 bytes and reports larger inputs explicitly.

Every new public language feature must bring checked-in `.sole-comb` examples
and relevant refusal cases. Its validation must invoke `./sole-comb`, including
actual stdout, stderr, and exit status. The native example census is explicit;
adding a file requires an expectation. `make check`, `make test`, and
`make gates` run this suite. The pinned `.kan` differential continues separately.

Next compiler work should extend this public path: complete erasure and the
backend for build/run, implement the M0 recursor semantics before enabling
recursive families, and add the remaining native forms with source tests.
The earlier bootstrap entry and separate test drivers are no longer the sole
evidence of progress toward a usable compiler.
