# Stage A.5b.3.2e.1: nominal family layouts

This increment plans the runtime layout of nominal recursive families and
their constructors. It does not erase constructor introductions or matches.

`erase/family.bend` ports `mu_layout` and `mu_group_tids` from `lib/erase.ml`
at clean Kanon revision `69f3be5198cda4334de4fd23b4ccc92cac595789`. The
planner resets the local scope, opens the family parameters as variables, and
evaluates each field type in an environment that the earlier fields extend.
It omits zero-quantity, type and proof fields. A constructor with no runtime
field has no field reprs. Constructor tags follow the declaration order of
the completed family. The constructor names come from `mu_ctor_names` in
`lib/kernel_rules.bend`, as Kanon calls `Rules.mu_ctor_names`.
`erase/type.bend` prints a family type as the nominal repr `mu<Name>`.

Commands:

```text
make test-family-layout
make test-family-layout-mutations
```

Both targets run `dev/test-family-layout.py`. The harness compiles the pinned
Kanon oracle fresh (`test/core-erasure-oracle.ml --layouts FILE FAMILY...`)
and compares its exact output with `test/family-layout.bend` on Bun, Node
worker and native. The mutation target runs the mutants on Bun only; the
harness refuses `--mutations` when `--hosts` does not include `bun`.

The harness checks 6 fixture files and 21 families. One fixture,
`test/family-layouts.kan`, is new; the other five are pinned Kanon fixtures
under `test/kanon/test/fixtures/`. Eleven independent goldens are hand-derived
literal lines that the pinned oracle output must contain. They cover the
indexed, parameterized, dependent, erased, function, product and mutual
cases. The `W` family binds a runtime type parameter, a proposition parameter,
a proposition field and a proof field, so its expected layout depends on the
order in which the field environment grows. Four isolated semantic mutations
target the constructor ordinal, ghost-field retention, the nominal name and
the per-field environment order. Each mutant must change the layout output
of `test/family-layouts.kan` without a failure.

Error texts for `Provisional` and `Builtin` families and for a missing
constructor are pinned by direct contracts in `test/core-erasure.bend`. These
texts were checked against Kanon `rules.ml` and `erase.ml:435`. They are not
compared differentially, because the oracle adapter reports errors on stderr.

The record `stage-a-family-layout.json` contains the oracle provenance, the
exact oracle output, the host observations, the mutation observations and the
source hashes. The source hashes cover the erasure sources, the harness, the
fixtures and the test-side import closure of `test/family-layout.bend`
(`test/program.bend` and the `test/pinfront/` files that it reaches). The
harness takes the hashes before validation and stops if they change before
it writes the record. The record `stage-a-family-layout-core-mutations.json`
holds the core-erasure mutation result from the review rerun. All 18
mutants are semantic rejections (18 of 18).

The gates logs and the public logs (four files) were recorded in a separate
worktree (`/Users/oobi/Documents/gpt5/sole-comb-recursive`) with the same
source hashes, before the review fixes. Their counts are the earlier counts.
The focused logs come from `make test-family-layout-mutations` in the review
rerun. They show the current counts.

- `stage-a-family-layout-gates.stdout.log` and
  `stage-a-family-layout-gates.stderr.log` hold a full `make gates` attempt.
  The attempt was stopped (`Terminated: 15`, exit 143) during the unchanged
  native test-program corpus comparison. Thus the program, erasure and
  core-erasure mutation gates did not run in that attempt.
- `stage-a-family-layout-public.stdout.log` and
  `stage-a-family-layout-public.stderr.log` hold a public run (house,
  test-cli, test-core-erasure and build). It passed with 217 command
  observations and 68 native files on each host. It wrote no new public
  record.
- `stage-a-family-layout-focused.stdout.log` and
  `stage-a-family-layout-focused.stderr.log` hold
  `make test-family-layout-mutations` from the review rerun. It passed with
  6 fixtures, 21 families and 11 goldens on Bun, Node worker and native, and
  4 semantic mutations on Bun.

Review rerun. The review ran these steps on a copy of the staged tree. All
steps exited 0:

- `make house` (0 s).
- `make test-core-erasure-mutations` (346 s): the core-erasure tests and 18
  mutations, 18 semantic rejections.
- `make test-family-layout-mutations` (135 s): the family layout tests and
  four mutations.
- `python3 -P dev/build.py --check` (2 s): the production build check.
- `python3 -P dev/test-cli.py` (98 s): the CLI suite on three hosts, with 68
  native files on each host and 217 command observations.
- `make test-elaboration-mutations` (143 s). It regenerated
  `stage-a-elaboration.json` and `stage-a-elaboration-mutations.json`,
  because `Makefile` and `dev/house-bend.py` changed.
- `python3 -P dev/pin-check.py` (0 s): the full pin check.

The program and erasure gates did not run again. They do not import
`erase/type.bend`, `erase/family.bend` or `erase/core.bend`.
`dev/test-erasure.py` imports only `erase/eterm.bend` and
`erase/runtime.bend`. `dev/test-program.py` uses `test/program-driver.bend`,
`lib/` and `test/pinfront/`. Thus this increment does not change their
inputs.

This extends [dependent-pair erasure](stage-a-pair-erasure.md). Runtime
constructor and match erasure for recursive families, recursive definitions,
complete erased-corpus integration and WebAssembly build/run remain pending.
