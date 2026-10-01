# Stage A.5b.3.2c: public finite-sum erasure

`./sole-comb check --erased examples/sum-erasure.sole-comb` erases finite-sum
layouts, injections and collection case elimination. Runtime tags keep their source
leg numbers after type or proof payloads drop. Every layout keeps a position
for each leg, using `unit` for an erased payload. Case branches emit in leg
order, with zero or one runtime binder according to quantity and payload type.
The scrutinee erases outside tail position; branch bodies retain tail position.

The public example covers type and empty-product payloads, nested product/sum layouts,
function payloads, closures capturing outer and payload binders, default arms,
explicit motives, zero-quantity branch binders, and empty case dispatch.
The existing finite-elim and records examples also support erased output.

`erase/sum.bend` derives the checked payload and branch plans. `erase/core.bend`
connects them to the existing fuel-spending term trampoline. Layout dependency
collection and runtime capture pruning use the existing tag and case traversal.
The implementation follows clean Kanon revision
`69f3be5198cda4334de4fd23b4ccc92cac595789`, especially `tag_intro`, `case_elim`
and `branch_of` in `lib/erase.ml`.

The production files contain 633 lines in `erase/core.bend`, 187 in
`erase/type.bend`, and 75 in `erase/sum.bend`. Their scoped line limits are
660, 190 and 90. The core limit grows by 60 lines for this increment; the type
limit stays at 190. Three new termination exemptions have individual reasons
in `dev/bend-policy.json`. Type, affine-use, exhaustive-match, purity and
production import-closure checks remain enabled.

This extends [product erasure](stage-a-product-erasure.md). Dependent pairs,
recursive runtime definitions, complete erased-corpus integration and WebAssembly
build/run remain pending. Applications whose remaining parameters all erase
retain the explicit refusal. A.5b.3.2 and A.close remain open.

## Validation

`make test-core-erasure test-core-erasure-mutations` passes on Bun, the Node
worker and native C. The [erasure record](stage-a-sum-erasure.json) binds
60 source hashes, 24 public observations across the three hosts, seven oracle
comparisons per host, and a passing run of the 20 direct contracts in
`test/core-erasure.bend` on each host. All
[14 mutations](stage-a-sum-erasure-mutations.json) produce semantic failures.
The [focused stdout](stage-a-sum-erasure-focused.stdout.log) records the
successful run; [stderr](stage-a-sum-erasure-focused.stderr.log) is empty.

`make test-cli` passes 56 files and 58 observations on each host, 181 command
observations in total. The
[public record](public-sum-erasure.json) binds 102 source hashes. Validation
runs in an isolated repository copy with Git metadata. The saved records are
checked against the working files and final staged blobs.

The standalone [pin check](stage-a-sum-erasure-pin.stdout.log) passes all
35 checks. The final [production build check](stage-a-sum-erasure-build.stdout.log)
also passes.

`make gates` stops with `make: *** [test-program] Error 1` when A.5b.2 native
code generation exceeds its existing 300-second limit (`emit-native`). The run passes A.1 through A.5b.1 on all
three hosts, then the A.5b.2 oracle, Bun and Node program checks: 245 files
in both modes. Later targets in that invocation were not reached. The
[gate stdout](stage-a-sum-erasure-gates.stdout.log) and
[gate stderr](stage-a-sum-erasure-gates.stderr.log) preserve this incomplete
run, including the timeout. The focused and public runs above complete
separately.

The differential harness compiles a fresh oracle from the clean semantic pin,
comparing each copied source with its revision's Git blob. Four independently
written `.kan` fixtures expand native products, records, function-valued arms,
default arms and `elim` into the pin's supported syntax. A fixed name mapping
restores qualified getters and the `Other.zero` postulate. The record discloses
both the fixture overrides and the mapping.

Seven public examples compare with the oracle. Twelve fixed sum goldens and
seven product goldens independently constrain runtime output. Twenty direct
contracts cover budget exhaustion, runtime admission, canonical emission of
reordered branches, and refusals for malformed addresses, payload counts,
branch binders and diagrams. Fourteen isolated mutations check that the suite
rejects errors in quantities, indices, captures, product fields and projections,
layout names, dependencies, sum tags and payloads, branch arity and selection,
and tail position.
