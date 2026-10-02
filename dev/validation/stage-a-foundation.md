# Stage A.1 foundation

Date: 2026-09-27. This is the first real compiler source milestone under
`lib/`, following the user's [entry decision](../stage-a-entry.md).
It ports the foundation contracts from Kanon
`69f3be5198cda4334de4fd23b4ccc92cac595789`.

The arithmetic core adapts the reviewed radix-32768 implementation from
attest's `lib/foundation.bend`, source SHA-256
`30dfb2d9d09081405072c49c99c50c0e5b5997dfe9447061fad2dc83cc1dcee3`.
It supplies signed arbitrary-precision addition and multiplication,
zero-clamped subtraction, strict unsigned decimal parsing, signed decimal
printing, comparison, and checked narrowing. This is compiler library
code; no scratch lexer or checker was copied into the repository.
The attest review does not cover the new sole-comb code: the signed
host-int adapter (`Int63.t`, `Bignum.of_int`, `Bignum.to_int`),
`Bignum.succ_int63`, text comparison, the ordered map, and their helpers.
The A.1 review below covers them. `Bignum.of_int` keeps the attest name,
but it takes a checked `Int63.t`, not a `Nat`.

| Module | Physical lines | Limit | Contract |
|---|---:|---:|---|
| foundation | 525 | 800 | Bignum operations, signed host-int adapter, shared pair, persistent ordered string map |
| kernel_budget | 24 | 40 | Finite fuel, explicit residual budget, typed exhaustion |
| kernel_error | 89 | 130 | All 15 pinned error constructors and their messages |
| kernel_level | 24 | 30 | Pinned constructors, comparisons, printing and signed-int successor wrap |
| kernel_literal | 18 | 20 | Pinned string and integer literals with exhaustive equality |

Bend's host Nat is bounded. Bignums therefore use normalized signed limb
lists; `to_nat` checks the 48-bit host range and `to_i31` checks the pin's
nonnegative 30-bit payload range. `to_int` returns a checked `Int63.t`
instead of silently truncating an OCaml integer into Nat. Levels retain
the pinned 63-bit signed successor wrap, including its extreme boundary.
Bend 2 has no abstract types. Thus `Level.t`, `Budget.t`, `Int63.t` and
`Map.t` show their representations, but the pin keeps `Level.t` and
`Budget.t` abstract. Use the module constructors and map operations to
preserve representation invariants. No check enforces this rule. The ordered map is a persistent sorted list with linear
operations; a balanced representation is not needed for A.1 correctness.

The finite budget replaces OCaml's effectful polling closure as required
by the host rules. A caller spends fuel and threads the returned value;
zero returns `Budget_exhausted`, and the kernel reads no clock. Exhaustion
placement in evaluation and checking belongs to A.3 and A.4.
`LInt` and `Termination` remain for Stage A pin compatibility. NAT-MU and
later semantic deltas have not been applied early.

After the review fixes below, `make gates` passed in the isolated
repository copy at
`/private/tmp/claude/kan-elim-lang-m0/review-0927-stage-a-foundation/gates-copy-postfix`:
the 143 existing Python regression tests, seven new policy tests, 35 pin
checks, the bootstrap entry check, and the library checks. The library
checks ran 219 independently expected cases on Bun, the pinned Node
worker, and native (INFO), with Bend type, affine-use and exhaustive-match
checks. The full outputs are [stdout](stage-a-foundation-gates.stdout)
and [stderr](stage-a-foundation-gates.stderr). No endpoint was selected. Expected arithmetic comes from Python integers;
this is focused behavioral evidence, not the complete OCaml differential.

The bounded review covered normalization, signed arithmetic, narrowing
boundaries, map ordering/replacement/removal, finite fuel, every error
message, level boundaries, and literal alternatives. It corrected the
level successor boundary and made test evidence fail closed by deleting
the previous result before a run. Two termination exemptions are
registered with decreasing-magnitude arguments; no catchalls are used.
The scoped policy also checks pure-source effects, loops, literal nonzero
divisors, stale registry entries, and the physical line limits.

[Recorded inputs and results](stage-a-foundation.json) pin the tested source
closure, runner, build and pin-check scripts, toolchain manifest, source
policy with its tests and registry, `Makefile`, and generated harness.
Detailed logs and generated programs remain under `_build/foundation/`.
No gate compares this record with a later run. The author's captures from
before the review fixes are outside the repository: the full gate run in
`/Users/oobi/Documents/gpt5/.kanon-exec/run-vk9F2D` and the final affected
checks in `/Users/oobi/Documents/gpt5/.kanon-exec/run-MHp4UT`.

A.2 representation is next. A.close, the complete 146-file KANON-DIFF,
full HOUSE, BUILD-TIME, and the remaining integration gates are pending.
R2 qualification and runtime selection remain pending at the operational
compiler gate. The bootstrap CLI is not yet an operational compiler.

## Review fixes, 2026-09-28

- The catch-all policy now checks each top-level pattern of a `case` row.
  Before, it found only a row with one bare binder, so a row such as
  `case _, Some{y}:` passed. A new policy test covers such rows.
- A new level case checks that `Level.equal` returns false for different
  levels. The case count changes from 218 to 219.
- The record now also pins the source policy, its tests, its registry,
  and the `Makefile`.
- The README row for `make test` now names all of its checks.
- This note now identifies the foundation code that is new in sole-comb,
  tells that Bend 2 cannot hide a representation, and keeps the gate
  outputs in the repository.

Current provenance: recorded Kanon fixtures; see no-ocaml-cleanup.md.
