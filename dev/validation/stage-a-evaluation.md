# Stage A.3 evaluation and conversion

Implemented on 2026-09-28, on top of A.2 commit
`58b99aa988834c6c6789c23f6dd2d97a67f42f9a`. This is a local compiler milestone
under the [milestone schedule](../stage-a-milestones.md). A.4 checking is next.
The complete Stage A integration gate and R2 remain pending.

## Source scope

`lib/kernel_eval.bend` implements evaluation, weak head reduction, and readback
for the thirteen term constructors and seven semantic value constructors at
Kanon `69f3be5198cda4334de4fd23b4ccc92cac595789`. It preserves closure binder
order, newest-first frozen spines, annotation probes and stored universe
levels, exact primitive saturation, opaque definitions, and constructor-guarded
unfolding. Raw readback does not implicitly force its input.

`lib/kernel_conv.bend` implements proof irrelevance, the pinned subsingleton
criterion, Pi and collection eta, structural comparison, and typed traversal
of neutral spines. Motives and branches are compared under their respective
captured environments and fresh binders. Binder names are irrelevant where
they are irrelevant at the pin. Conversion uses an abstract checker interface;
the real context, type inference, and budget consumption remain A.4 work.

The runtime and conversion operations in `kernel_rules.bend` move forward
from A.4 because A.3 needs beta reduction, eta, diagram comparison, and spine
types. Quantity equality and signed host index arithmetic accompany them.
The quantity algebra, checking rules, positivity checking, printing, and
specification counts remain A.4 deliverables. No Stage B semantic delta is
included. The pin's SPar, SNu, Auto, and mu right-former refusal paths remain.

Evaluation and conversion suspend recursive work as typed requests with
linear result continuations. This accommodates Bend's declaration ordering
without mutually recursive module definitions. The dispatcher returns both
success and failure through the continuation, so an annotation probe can
recover from a failed nested evaluation. Static callbacks carry their runtime
context explicitly. Shape dispatch remains in the rules module.

Four host recursion sites are registered in `../bend-policy.json`: `pbind`,
`pcatch`, `run`, and `coll_eta_loop`. Their reasons and limits are explicit.
Type, affine-use, and exhaustiveness checking remain enabled. Unchecked raw
terms can diverge as in the pinned evaluator. No object-language unsafe
construct is added, and no existing gate or line budget is weakened.

## Validation and bounded review

Command: `make gates`, run after the review fixes in an isolated copy of the
staged tree that keeps `.git`. Exit status: 0.

| Check | Result |
|---|---|
| Existing Python harness and policy tests | 150 passed |
| HOUSE | 16 host files, 15 line budgets, 7 registered unsafe sites, 0 catchalls |
| A.1 regression | 219 cases per endpoint |
| A.2 regression | 465 cases per endpoint |
| A.3 differential | 251 cases per endpoint, 98 independent expected results |
| Pin checks | 35 passed, 0 failures, 0 warnings |
| Host entry build check | Passed |

All three behavioral suites passed on Bun, the configured Node worker, and
native execution. Native remains informational for host qualification; a
native failure still fails these local scripts. `make test-evaluation` runs
A.3 alone. `make check`, `make test`, and `make gates` include it.

The A.3 oracle is freshly compiled with OCaml 5.2.1 and Zarith 1.14 from a
minimal dependency closure copied into `_build/evaluation/oracle`. The copy
includes the pinned interface file of each module that has one. Every copied
source is compared byte-for-byte with its Git object at the configured Kanon
revision before compilation. The Kanon checkout is never rebuilt or
modified. This avoids relying on the prebuilt oracle executable whose build
revision remains unverified in `toolchain.json`.

The paired generator supplies explicit syntax, semantic values, environments,
and globals to both implementations. Independent observers serialize complete
readback structure or exact typed errors. The test context supplies identical
conversion callbacks on both sides; its injected exhausted-budget result
checks propagation and failed-probe fallback, not A.4's future budget accounting.
The runner requires exactly 251 distinct cases, complete ordered oracle
output, passing independent expectations, and an exact success marker from
each endpoint. A failed run removes its old result record first.

Coverage includes beta reduction for Pi, collections and mu; closure and
argument order; stored annotations; all primitives, partial application and
refusal; successful and blocked recursive guards; signed index wrapping;
eta expansion; carried universe levels; typed point arguments; captured
motives, branch addresses, and second branches; subsingleton eligibility;
and short-circuit error order.

The bounded review covered the new modules, their rules and arithmetic
dependencies, the paired adapters, gate wiring, source policy, and evidence.
The differential run exposed use of saturating natural subtraction in signed
host arithmetic. Signed subtraction now adds the negated operand and wraps
at 63 bits. Negative lookup, readback, boundary, and deterministic arithmetic
cases exercise the fix. Review also replaced ineffective recursive-guard
fixtures with constructor arguments and added eta and subsingleton cases.

Tracked evidence:

- [Result and input hashes](stage-a-evaluation.json), including oracle source,
  compiler, generated adapter, executable, observations, and case identities.
- [Full gate stdout](stage-a-evaluation-gates.stdout.log) and
  [stderr](stage-a-evaluation-gates.stderr.log).
- [Oracle observations](stage-a-evaluation-oracle.tsv).

Historical A.1 and A.2 validation records remain unchanged. The regression
results above and current source hashes belong to this A.3 run.

## Review fixes, 2026-09-28

A staged review found three minor defects. All three are fixed.

- The case `former-refusal-mu` had a refusal name, but the pin converts two
  mu right formers without refusal. The case is now `former-mu-converts`
  with the fixed expectation `true`. The case `out-mu-refused` still covers
  the mu refusal.
- The oracle copied and compared only `.ml` files. It now also copies and
  compares the pinned `.mli` file of each needed module and compiles it
  before its implementation. Because `level.mli` makes `Level.t` abstract,
  the adapter now builds each universe level with the pinned `Level.of_int`.
  The generator refuses a negative level, because the pinned interface
  cannot build one. The oracle observations did not change.
- The suggested commit command now includes `-s` for the sign-off.

## Remaining boundaries

This is a raw kernel adapter, not the complete 146-file, three-mode Kanon
frontend differential. Full KANON-DIFF, Stage A mutant gates, compiler checking,
BUILD-TIME qualification, and R2 remain pending. A.3 passing does not close
Stage A or authorize Stage B.

On malformed raw values, negative collection eta widths return an explicit
mismatch instead of the OCaml pin's `List.init` exception. Checked collection
widths are nonnegative. This boundary is not counted as checked-term parity.
The abstract test callbacks do not establish typechecking soundness or
resource bounds for the future operational compiler.

Suggested user commit after review:

```sh
git -C /Users/oobi/Documents/sole-comb commit -s -m 'lib: add Stage A.3 evaluation and conversion'
```
