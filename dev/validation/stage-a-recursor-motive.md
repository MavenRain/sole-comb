# A.5b.3.2e.11: checked recursor motives

This increment follows uniform recursive-parameter validation at `67a0356`.
`lib/kernel_recursor_motive.bend` checks a motive and computes the type of
each direct recursive child's induction hypothesis. Public branch binders,
branch quantities, delayed recursive evaluation and erasure remain pending.
UAT.0 and Stage A acceptance stay open.

## Contract

`layout` first obtains a uniform recursive-field layout. It opens the family
parameters in a fresh declaration context, using the caller's globals and
budget. Ambient locals and constructor fields are excluded from the motive's
scope. A core motive binds the declared indices in order, then the scrutinee.
The existing kernel rules check its family name, index count, dependent index
domains and result universe. The existing restriction on large elimination
from a proposition also applies, including constructors without recursive
children. Parameter diagrams preserve declaration order.

For each direct recursive field, the planner applies the checked motive to
that child's indices and value. It evaluates the motive in the declaration
parameter environment. The returned semantic type is interpreted in the raw
layout's complete field context, so indices and self-dependent results can
refer to constructor fields without capturing them as motive parameters.
Ordinary fields produce no hypothesis. Recursive children retain declaration
order, including children with quantity zero.

The result contains the original layout, the motive universe and a separate
list of hypothesis positions, names and types. It does not bind hypothesis
variables, assign their quantities, check branch bodies, or evaluate recursive
calls. A checked type plan is not a termination certificate or public recursor
implementation. The existing public compiler semantics remain unchanged.

## Validation scope

The dedicated harness uses freshly checked declarations and fixed expected
output. Existing golden layouts supply the unchanged field metadata; new
expectations specify hypothesis types, motive levels and refusals. Cases
cover multiple and zero-quantity children, parameter order and scope,
dependent parameters, aliases, dependent indices, swapped child indices,
self-dependent motives, malformed motives and proposition elimination.
The existing nonuniform, mutual and nondirect layout refusals are also checked.
The driver refuses an unknown motive mode, and a case checks that refusal.

`check` includes `test-recursor-motive`; `test` and `gates` include
`test-recursor-motive-mutations`. The full `make gates` suite is not claimed
for this increment. The existing M0 acceptance obligations remain required.

## Results

All 43 motive cases pass on Bun, Node worker and native. The 19 raw-layout
cases and 34 uniform-layout cases, plus the uniform harness's unknown-mode
refusal, pass on Bun and Node worker. Host runs perform fresh Bend checks and
compilation.

Eight compiled behavioral mutations cover omitted motive checking, omitted
family checking, omitted proposition restrictions, incorrect parameter
diagrams, reversed child indices, incorrect child values, lost parameter
environments and skipped later hypotheses. Each mutant must typecheck and
compile before the harness accepts a clean behavioral mismatch as a kill.
All eight mutations are killed on Bun.

The source policy and 26 Python tests pass: 57 scoped files, 45 line budgets,
63 registered unsafe sites and 41 catchalls. This module adds no unsafe or
catch-all sites. The production driver passes `dev/build.py --check`, and
all 34 toolchain checks pass. The Makefile graph preserves the default
`build` target and every existing `check`, `test` and `gates` prerequisite.

The motive suite passes on all three hosts with a fresh native build. This run
took 204 seconds and its peak memory was 3827 MB. Before the unknown-mode
refusal was added, the suite passed 42 cases on all three hosts and killed all
8 mutants. That run took 800 seconds and its peak memory was 3639 MB. The
raw-layout native build timed out after 900 seconds under host load. Its log
is retained beside this report. The raw-layout and uniform-layout suite
sources are byte-identical to `67a0356`, except the Makefile. The staged
Makefile change only appends the motive targets. The `67a0356` records
[stage-a-recursor-uniform-layout-raw.json](stage-a-recursor-uniform-layout-raw.json),
[stage-a-recursor-uniform-layout.json](stage-a-recursor-uniform-layout.json)
and [stage-a-recursor-layout.json](stage-a-recursor-layout.json) hold the
three-host passes. The default Make targets still request all three hosts and
preserve the original timeout behavior.

The motive run used the unchanged harness through
`make test-recursor-motive-mutations`.
The [motive record](stage-a-recursor-motive.json),
[raw record](stage-a-recursor-motive-raw.json),
[uniform record](stage-a-recursor-motive-uniform.json) and
[integration record](stage-a-recursor-motive-integration.json) retain the
observations, source hashes and Makefile comparison. Validation ran in an
isolated checkout. Publication verifies the recorded inputs against the
destination and staged blobs.
