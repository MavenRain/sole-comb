# A.5b.3.2e.12: checked recursor branch types

This increment follows checked recursor motives at `20c18b6`.
`lib/kernel_recursor_branch.bend` constructs the binder and target types of
one structural recursor branch and checks them with the kernel. Branch
bodies, public elimination hypotheses, delayed recursive evaluation and
erasure remain pending. UAT.0 and Stage A acceptance stay open.

## Contract

`layout` first obtains the checked motive plan of
[A.5b.3.2e.11](stage-a-recursor-motive.md) for the constructor. It opens the
family parameters in a fresh declaration context, with the caller's globals
and budget. Ambient locals are excluded from the branch scope.

The branch binders are the constructor fields in declaration order, then one
hypothesis binder for each direct recursive field, also in declaration order.
A field binder keeps the quantity and domain of its field. The k-th
hypothesis binder takes the k-th direct recursive child and the hypothesis
type of that child from the motive plan. Hypothesis positions continue after
the last field.

The quantity of a hypothesis follows from the quantity of its child. A zero
child gives a zero hypothesis. A many child gives a many hypothesis. The
computation of a hypothesis consumes its child, so an affine child cannot
also be used in the branch body. For an affine recursive child, the planner
returns a `Not_yet` refusal. Affine ordinary fields stay unchanged.

The target is the ordinary elimination target. The planner evaluates the
declared constructor indices in the parameter and field environment. It
applies the motive, in the parameter environment, to those indices and to
the constructor value over the field variables.

The kernel checks each binder domain as a type in its own prefix context:
the parameters, the earlier fields and the earlier hypotheses. It checks the
target as a type after all binders. The result holds the motive plan, the
complete branch context, the binders and the target. The planner does not
check a branch body, bind public elimination hypotheses, or evaluate a
recursive call. The public compiler semantics remain unchanged.

## Design rulings

1. Hypothesis binders follow every field.
2. The k-th hypothesis takes the k-th direct recursive child.
3. A zero child gives a zero hypothesis. A many child gives a many hypothesis.
4. The planner refuses an affine recursive child with `Not_yet`.
5. The target is the ordinary elimination target. The test driver compares
   it with the target that the kernel's `mu_bind_fields` and `mu_result`
   construct for the same constructor.
6. The kernel checks each domain in its own prefix and the target after
   every binder.

## Validation scope

The dedicated harness uses freshly checked declarations and fixed expected
output. The driver binds an ambient local before it calls the planner. The
reported context size shows if the branch scope captures that local.

Cases cover nullary and direct constructors, multiple children, zero and
affine children, affine ordinary fields, later fields, ordinary, recursive,
dependent and alias parameters, parameter order, indexed and two-index
families, dependent indices, self-dependent motives, proposition elimination
and malformed motives. They also cover unknown constructors, undeclared
families and the existing nonuniform, mutual and nondirect layout refusals.
Two certificate probes give the kernel check an ill-formed target and an
ill-formed binder. The driver refuses an unknown motive mode, and a case
checks that refusal.

`check` includes `test-recursor-branch`; `test` and `gates` include
`test-recursor-branch-mutations`. The full `make gates` suite is not claimed
for this increment. The existing M0 acceptance obligations remain required.

## Results

All 44 branch cases pass on Bun and Node worker. Host runs perform fresh Bend
checks and compilation. This run took 16 seconds and its peak memory was
1770 MB.

Nine compiled behavioral mutations cover hypotheses placed before fields, a
zero child given a many hypothesis, an admitted affine child, reversed target
indices, dropped constructor fields, skipped formation checks, wrong
hypothesis positions, wrong child lookup and skipped later hypotheses. All
nine mutations are killed on Bun. Each killed mutant exited cleanly with output that differs from the
expected output. This run took 118 seconds and
its peak memory was 2589 MB.

The native host is not validated. A 4000 MB memory guard stopped the
three-host mutation run at 4276 MB after 22 seconds. It then stopped a
native-only run at 4349 MB after 12 seconds. The native log is retained
beside this report. The default Make targets still request all three hosts.
A native pass remains open.

The source policy and 26 Python tests pass: 58 scoped files, 46 line budgets,
63 reviewed unsafe sites and 41 catchalls. This module adds no unsafe or
catch-all sites. The production driver passes `dev/build.py --check`, and
all 34 toolchain checks pass. The Makefile change only appends the branch
targets. The house policy adds one line budget for the new module.

The motive, raw-layout and uniform-layout suites were not rerun, because this
increment changes no shared kernel module or existing harness. The Bun and
Node worker run used `dev/test-recursor-branch.py --hosts bun,node-worker`.
The mutation run used `dev/test-recursor-branch.py --hosts bun --mutations`.
The [branch record](stage-a-recursor-branch.json) and
[mutation record](stage-a-recursor-branch-mutations.json) retain the
observations and source hashes.
