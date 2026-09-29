# Stage A.5b.1: expression and declaration elaboration

This increment ports the expression, ordinary declaration, and mutual family
entry points from Kanon `69f3be5198cda4334de4fd23b4ccc92cac595789` into the
test-only frontend. It covers every surface expression constructor, local and
global lookup, dependent binders, collection eliminations, constructors,
field annotations, and family admission through the existing kernel checker.

`test/pinfront/elab.bend` exposes `elab`, `elab_decl`, and `elab_mu_group`.
The suspended requests in `elab_program.bend` carry immutable contexts and
typed errors, following the existing kernel request/resume pattern. The host
policy records three additional unsafe sites and six fallback functions.
Type checking, affine-use checks, exhaustive result handling, and the
production import boundary remain enforced.

## Comparison boundary

The runner recompiles the pinned OCaml kernel and frontend from verified
source bytes. Both observation adapters parse each input and process ordinary
declarations and mutual families in order. They compare the exact printed
raw kernel declarations or the first exact error. This is an entry-point
differential, not the complete program or CLI differential.
Family admissions emit `mu NAME;` rows; later declarations and focused
family probes exercise the resulting environments.

There are 221 inputs: all 146 pinned corpus files plus 75 focused probes.
Of the probes, 65 have an independent fixed output or verdict prefix in
addition to exact oracle comparison. They include local shadowing, binder
depths, large naturals, tuple and sum widths, pair projections, dependent
motives, typed constructor fields, positivity failures, mutual families,
and declaration-level recursive-group refusal.

The pinned dependent pair projection behavior is preserved: an elimination
without a motive can require an expected type. A separate probe checks
dependent pair introduction.

The Bend adapter gives each kernel query a fresh budget of 16384 inference
steps. The kernel queries are inference, checking, universe inference, type
conversion, declaration check, family declaration, and constructor
definition. Family admission also gives the same fresh budget to each family
step. The request recursion of the elaborator has no step limit. The pinned
OCaml kernel has no budget, so a single query over the limit fails in Bend
but the pin accepts it. No case reaches the limit. An exhausted budget
appears as an error observation that the comparison reports, not as a hidden
result. These checks do not establish equivalence for every finite budget or
arbitrary malformed internal syntax values.

## Reproduction and evidence

```sh
make test-elaboration
make test-elaboration-mutations
make gates
make build
```

The normal gate builds a fresh oracle and checks Bun, Node worker, and native
C. The argument driver avoids embedding the whole corpus in the generated
program. Native uses the existing informational host convention and still
must agree on every observation. Native C compilation keeps `-O3` and has a
900-second build limit; individual observation runs retain a 300-second
limit. `--replay` requires matching pin, runner,
adapter, corpus identity, and observation encoding before consuming the
saved reference.

The existing A.4 checker harness also uses the 900-second native build
allowance. Its original 120-second compilation limit expired twice during
this validation. Its execution limits, comparison assertions, and mutation
requirements are unchanged. This is a compilation allowance, not a
performance-gate result.

Three isolated, executable mutations remove local-name equality, change the
second pair projection to the first, and bypass field-annotation conversion.
Compilation or transport failure does not count as detecting a mutation.
The mutation runner requires a successful complete gate over the exact
baseline sources and checks that those sources remain unchanged.

Machine-readable observations and validation records are stored in
`stage-a-elaboration-reference.json`, `stage-a-elaboration.json`, and
`stage-a-elaboration-mutations.json`. Full command output stays under
`_build/elaboration/` when the checks are run.

## Recorded validation

Validated on 2026-09-28. All 221 elaboration observations agree on Bun,
Node worker, and native C, with zero divergences. All three new mutations
compile, execute, and disagree with the baseline as intended. The result and
mutation records match the 39 recorded source hashes.

`make gates` passed on 2026-09-28 with these results, in one uninterrupted
run on an isolated copy of the staged tree that keeps `.git`:

| Check | Result |
| --- | --- |
| A.1, A.2, A.3, A.4, A.5a regressions | 219, 465, 251, 291, and 561 cases respectively on Bun, Node worker, and native INFO |
| A.4 behavioral mutations | All three rejected |
| A.5a behavioral mutations | All five killed by observation mismatches |
| A.5b.1 elaboration comparisons | All 221 cases agree on Bun, Node worker, and native INFO; 65 independent expectations pass |
| A.5b.1 behavioral mutations | All three killed by observation mismatches |
| Harness failure injection | Nine tests pass |
| HOUSE | Fifteen tests pass; 26 files, 18 line budgets, 18 reviewed unsafe sites, 11 catchalls |
| Pins and host build check | All 35 pin checks and the final host check pass |
| Earlier build, benchmark, and R2 harness regressions | All pass |

The [result record](stage-a-elaboration.json) and
[mutation record](stage-a-elaboration-mutations.json) bind the checks to
source hashes. Full gate output is retained in
[stdout](stage-a-elaboration-gates.stdout.log) and
[stderr](stage-a-elaboration-gates.stderr.log). The oracle run compared its
fresh cases with the saved
[reference observations](stage-a-elaboration-reference.json). The
`reference_sha256` field of the result record binds the saved copy by hash.

## Review fixes, 2026-09-28

- Each oracle run now compares every fresh case with the saved reference and
  writes the fresh copy under `_build/elaboration/`. Before this fix, the
  gate replaced the saved reference without a comparison, so a changed oracle
  observation did not stop it. The mutation runner now reads the fresh copy
  that the gate compared.
- The recorded source hashes now include `dev/build.py`, which supplies the
  compiler resolution and the Node worker host.
- This record now states that the kernel budget applies to each kernel
  query, not to each exposed call, and that the request recursion of the
  elaborator has no step limit.
- This record now shows one complete `make gates` pass with in-repository
  logs. Before this fix, it described an interrupted run, a partial rerun,
  and captures in an unpinned external checkout.
- This record no longer contains a suggested commit command.
- The gate rerun used an isolated copy of the staged tree that keeps `.git`.
  The result, reference, and mutation records and the gate output come from
  that rerun.

## Remaining integration

A.5b.2 must add the recursive program driver with order and totality checking,
full frontend execution, all required output and exit modes, and complete
KANON-DIFF coverage. A.5b.1 does not close Stage A or change its acceptance
gates, divergence policy, benchmark admission, or performance claims.
