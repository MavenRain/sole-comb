# Stage A.5b.3.2a: public ordinary erasure

`./sole-comb check --erased examples/erasure.sole-comb` checks native source
and prints type-directed erased definitions. This bounded increment covers
Nat values, axioms, definitions, ordinary functions, partial applications,
eta expansion, ghost parameters, linear parameters, and lifted closures.
Universes, proof positions and empty products disappear. Runtime context
slots preserve variable indices after drops, and runtime capture pruning
removes captures used only in erased arguments.

Nonempty structural runtime layouts, structural values and eliminators,
recursive runtime definitions, full erased-corpus integration and WebAssembly
build/run remain pending. A recursive or partial definition at a nonruntime
type drops before its recursion is examined, as in Kanon. An application
whose remaining parameters all erase reports an explicit erasure refusal
instead of a saturated call. This increment does not close A.5b.3.2 or
A.close.

`erase/type.bend` and `erase/core.bend` add 654 production lines, with scoped
line limits of 160 and 550. Their termination exemptions have individual
reasons in `dev/bend-policy.json`; type, affine-use and exhaustive checks
remain enabled. The production import closure stays separate from pinfront.

## Validation

The saved [core record](stage-a-core-erasure.json) binds 52 input hashes to
a fresh oracle compiled from clean Kanon revision
`69f3be5198cda4334de4fd23b4ccc92cac595789`. Every copied oracle source is
compared with the revision's Git blob before compilation. No cached Kanon
executable supplies the expected output.

- Three native examples match the oracle's erased output exactly on Bun,
  Node worker and native. The new example contains 19 checked declarations.
- Two successfully checked structural examples explicitly refuse erasure
  on each host, with failure status and no success output.
- A successfully checked application whose remaining parameters all erase
  explicitly refuses erasure on each host, with failure status and no
  success output.
- Six direct contracts cover proof and universe drops, Nat retention,
  zero and linear quantities, and exhausted erasure fuel on all three hosts.
- The [mutation record](stage-a-core-erasure-mutations.json) records four
  isolated semantic rejections: retained zero arguments, retained proofs,
  shifted variable indices, and unpruned captures. Build failures do not
  count as mutation rejection.
- The [public record](public-core-erasure.json) covers 54 native files and
  175 observations on all three hosts, including existing lexical boundaries,
  refusals, sugar equivalences and record output.

`make test-cli test-core-erasure-mutations` passes. The scoped house policy,
R2 harness tests, benchmark harness, build harness and pin-check harness also
pass. Gate continuations stopped twice in the unchanged foundation native
compile at its 120-second test deadline; Bun and Node worker foundation
checks passed. Full `make gates` is therefore not recorded as passing.
The benchmark timeout test initially failed its child startup assertion;
its isolated check and subsequent complete benchmark suite passed unchanged.

Raw command and host logs remain under `_build/core-erasure`,
`_build/core-erasure-mutations` and the local managed-command captures.
The saved records preserve the observations and source hashes needed for
review without importing build products into the repository.

## Bounded review

Reviewed the public entry diff, all new production declarations, type-driven
argument admission, runtime context indexing, eta expansion, closure capture
ordering and pruning, fuel handling, oracle provenance, and mutation failure
classification. Structural refusals remain explicit. Required full-corpus,
Wasm, A.close and R2 performance acceptance are still pending.
