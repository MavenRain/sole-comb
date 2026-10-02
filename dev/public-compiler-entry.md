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
Elim. Each supported arm is a function term. Lambda annotations and inferred
function domains are checked against the corresponding payload type. Motives
are supported.

The default-arm increment adds final `else: term` syntax to finite sums.
`surface/sugar.bend` expands it into ordinary positional arms before branch
elaboration. The checked scrutinee's collection determines the remaining
payloads, and each expanded function is checked independently. Defaults must
cover at least one remaining leg; duplicate or non-final defaults fail.
The public suite compares default examples with independently written explicit
expansions, including motives, local and linear functions, and nested captures.

The closed-record increment adds `record Name : Type i { field : T; ... }`.
It expands to a product alias and qualified getter definitions such as
`Name.field`. Apply a getter to a tuple or record value to project its field.
Field types use the preceding global scope. Empty, nested, heterogeneous,
and higher-universe products are supported; each generated definition goes
through the ordinary elaborator and kernel checker.

The supported prefix includes ordinary definitions, axioms, dependent
functions, Nat primitives, sums, products, closed records, and finite-sum elimination.
Recursive-family recursors, constructor-labelled arms,
dependent or parameterized records, complete type-directed erasure, and WebAssembly build/run remain
pending. Unsupported forms must fail, and native examples for them must enter
the public suite when they become operational. The first increment's source
transport accepts files up to 32768 bytes and reports larger inputs explicitly.

Every new public language feature must bring checked-in `.sole-comb` examples
and relevant refusal cases. Its validation must invoke `./sole-comb`, including
actual stdout, stderr, and exit status. The native example census is explicit;
adding a file requires an expectation. `make check`, `make test`, and
`make gates` run this suite. The pinned `.kan` differential continues separately.

The ordinary-erasure increment adds `check --erased` to this public path.
It checks the source first, then erases Nat values and ordinary functions using
the checked types. It drops ghost parameters, universes, proofs and empty
products, handles eta expansion and partial applications, lifts closures and
prunes unused captures. That increment refused structural runtime layouts and
recursive definitions. Pinned-reference fixture comparisons and direct contracts
run on Bun, Node worker and native; four isolated semantic mutations exercise
the new gate. See [core erasure validation](validation/stage-a-core-erasure.md).

The product-erasure increment adds collection tuples and their projections,
including closed-record layouts and generated getters. Type and proof fields
drop before runtime field numbering; nested products and function fields retain
their checked layouts and captures. Declaration groups collect layouts mentioned
inside function bodies as well as signatures. A separate explicit product
fixture supplies the archived reference input. See
[product erasure validation](validation/stage-a-product-erasure.md).

Next compiler work should extend this public path: complete erasure and the
backend for build/run, implement the M0 recursor semantics before enabling
recursive families, and add the remaining native forms with source tests.
The earlier bootstrap entry and separate test drivers are no longer the sole
evidence of progress toward a usable compiler.
