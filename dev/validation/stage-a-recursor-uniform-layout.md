# A.5b.3.2e.10: uniform recursive parameters

This increment follows the direct recursive-field layouts at
`6c14c175fddb850bece29de71c1cf296c3996d36`. It adds checked parameter
compatibility metadata in `lib/kernel_recursor_uniform.bend`. Public recursor
branch typing, motives, delayed induction hypotheses and erasure remain
pending. UAT.0 and Stage A acceptance stay open.

## Contract

`layout` obtains the existing raw constructor plan, which opens the declaration
parameters and fields in a fresh context. It validates every direct recursive
field, including fields with quantity zero. Ordinary fields retain the raw
planner's classifications and refusals.

The validator opens the parameter diagram of each child, restores declaration
order from the reversed semantic environment, and compares every argument with
the corresponding fresh declaration variable. It evaluates each parameter
domain in the declaration prefix, so later domains can depend on earlier
parameters. Typed conversion runs in the complete field context, since a
changed child argument can mention a constructor field. Transparent aliases
and proof-irrelevant arguments follow the existing typed conversion rules.

A changed argument returns `not yet: recursor layout requires uniform
recursive parameters`. A different parameter arity or a missing family former
returns an explicit mismatch. The returned plan retains the raw fields and
indices. This validation does not establish a recursor typing certificate,
branch quantities, motive admissibility or termination of a generated recursor.

## Validation

`dev/test-recursor-layout.py --uniform --mutations` passes 34 fixed cases on
Bun, Node worker and native. An accepted case prints the same plan report as
the raw suite, so the golden output also checks that the validator returns the
raw fields and indices. Cases cover the existing layout acceptance and refusal
paths, swapped and changed parameters, constructor-field parameters, dependent
domains, transparent aliases, value parameters, zero-quantity children, proof
irrelevance, a changed parameter on a later recursive child and indexed
families with parameters. Each host also refuses an unknown fourth argument.
Six separately typechecked and compiled mutants fail their behavioral
witnesses on Bun: ignored conversion results, reversed parameter order,
incorrect declaration levels, skipped child checks, a check of the first
recursive child only and untyped conversion replacing typed conversion.

The 19 raw-layout cases also pass on all three hosts with a fresh compile.
`make test-recursor-layout-mutations` ran them and the existing mutation suite,
which detects all five raw-layout mutations on Bun.

`make house test-reference-fixtures` passes 26 Python tests and the scoped
source policy: 56 files, 45 line budgets, 63 reviewed unsafe sites and 41
catchalls. The new module adds no unsafe sites or catchalls. The production
driver passes `dev/build.py --check`; all 34 toolchain pin checks pass.

The Makefile comparison preserves the default `build` target and every
existing prerequisite of `check`, `test` and `gates`. `check` adds
`test-recursor-uniform`; `test` and `gates` add its mutation target.
The complete `make gates` suite was not rerun for this increment.

The [uniform record](stage-a-recursor-uniform-layout.json),
[raw regression record](stage-a-recursor-uniform-layout-raw.json) and
[Makefile record](stage-a-recursor-uniform-layout-integration.json) retain
observations, source hashes and integration evidence. Adjacent stdout and
stderr logs retain the successful commands. Validation ran in an isolated
copy; publication verifies the recorded inputs against the destination and
the staged blobs.
