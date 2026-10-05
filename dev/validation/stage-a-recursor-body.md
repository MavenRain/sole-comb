# A.5b.3.2e.13: checked recursor branch bodies

This increment follows checked recursor branch types at `0a648e0`.
`lib/kernel_recursor_body.bend` checks the body of one structural recursor
branch against its branch plan. Constructor coverage, ambient captures,
public elimination hypotheses, delayed recursive evaluation and erasure
remain pending. UAT.0 and Stage A acceptance stay open.

## Contract

`check` first obtains the branch plan of
[A.5b.3.2e.12](stage-a-recursor-branch.md) for the constructor. A plan
refusal is returned unchanged. This includes a motive refusal, an unknown
constructor and the `Not_yet` refusal for an affine recursive child.

The leg must bind one name for each plan binder: the constructor fields in
declaration order, then the hypotheses. If the counts differ, `check`
returns a `Missing_branch` refusal that gives the constructor and the two
counts.

Each leg binder has a quantity mark. The mark must agree with the quantity
of its plan binder. If it does not, `check` returns the kernel
`binder_quantity` refusal. The refusal gives the leg name, the two
quantities and the source of the plan binder: a field or a hypothesis.
`check` reports the first disagreement in binder order.

`check` then builds the body scope. It opens the family parameters in a
fresh declaration context, with the caller's globals and budget. It binds
each leg name with the quantity and the domain of its plan binder. The
levels are thus the levels of the plan, and the plan target stays valid.
Ambient locals are excluded from the body scope.

The kernel checks the body against the plan target at the runtime mode. An
erased mode stays erased. Each other mode becomes mode one. The kernel then
closes the usage back to the parameter scope with the caller's mode. The
check refuses a runtime read of an erased binder. In a runtime mode, the
close refuses a linear binder that is not used exactly one time on every
runtime path. The result is the usage of the family parameters.

`check` does not check constructor coverage, capture ambient locals, bind
public elimination hypotheses, or evaluate a recursive call. The public
compiler semantics remain unchanged. No other kernel module changes.

## Design rulings

1. The leg binds every field, then every hypothesis. The count must equal
   the plan count.
2. Each leg quantity must agree with the plan quantity. The first
   disagreement in binder order is reported.
3. The leg gives the names. The plan gives the quantities and the domains.
4. The body scope starts at the declaration parameters. Ambient locals are
   excluded.
5. The body is checked at the runtime mode. The close uses the caller's
   mode and goes back to the parameter scope.
6. A plan refusal comes first and is returned unchanged.

## Validation scope

The dedicated harness uses freshly checked declarations and fixed expected
output. The driver binds an ambient local before it calls `check`. One case
reads past the leg binders and the parameters. Its refusal shows that the
body scope does not capture that local.

No public syntax exists for a branch leg. The driver takes the leg binders
as quantity and name pairs. It takes the body from a fixed table of core
terms: variables and constructor applications. A `result` motive gives the
constant motive of a declared family `Out`, so a body can be a constructor
application. An accepted case prints the parameter count and the names of
the parameters that have a runtime read after the close.

Accepted cases cover nullary and direct constructors, a body that uses the
field and the hypothesis, a dependent motive, recursive and ordinary
parameters, an indexed family whose target uses the constructor indices,
multiple children, a zero hypothesis used only in an erased position,
linear fields used one time, the erased checking mode and a runtime read of
a parameter.

Refused cases cover too few and too many leg binders, a binder on a nullary
constructor, a leg that omits the hypotheses, field and hypothesis quantity
disagreements, bodies of the wrong type, an erased field and an erased
hypothesis read at runtime, a linear field used two times and a linear
field not used, and a read of the ambient local. They also cover the
propagated refusals: an affine child, an unknown constructor, an undeclared
family and four malformed motives. The driver refuses an unknown motive
mode, and a case checks that refusal.

`check` includes `test-recursor-body`; `test` and `gates` include
`test-recursor-body-mutations`. The full `make gates` suite is not claimed
for this increment. The existing M0 acceptance obligations remain required.

## Results

All 46 body cases pass on Bun and Node worker. Host runs perform fresh Bend
checks and compilation. The expected outputs were written from the kernel
source before the first run, and the first run matched all 46. This run
took 16 seconds and its peak memory was 1736 MB.

Nine compiled behavioral mutations cover a dropped binder count guard, a
skipped quantity agreement, a swapped binder source, a skipped close, a
body checked in the erased mode, a body checked in the caller's mode, a
close in the erased mode, a close to the wrong size and an ambient origin.
All nine mutations are killed on Bun. Each killed mutant exited cleanly with
output that differs from the expected output. This run took 77 seconds and
its peak memory was 2819 MB.

The native host is not validated. A 4000 MB memory guard stopped a
native-only run at 4592 MB after 10 seconds. The run was not repeated. The
native log is retained beside this report. The default Make targets still
request all three hosts. A native pass remains open.

All 44 branch cases of A.5b.3.2e.12 pass again on Bun and Node worker. That
run took 14 seconds and its peak memory was 2499 MB.

The source policy and 26 Python tests pass: 59 scoped files, 47 line
budgets, 63 reviewed unsafe sites and 41 catchalls. This module adds no
unsafe or catch-all sites. The production driver passes
`dev/build.py --check`, and all 34 toolchain checks pass. The Makefile
change only appends the body targets. The house policy adds one line budget
for the new module.

The motive, raw-layout and uniform-layout suites were not rerun, because
this increment changes no shared kernel module or existing harness. The Bun
and Node worker run used `dev/test-recursor-body.py --hosts bun,node-worker`.
The mutation run used `dev/test-recursor-body.py --hosts bun --mutations`.
The [body record](stage-a-recursor-body.json) and
[mutation record](stage-a-recursor-body-mutations.json) retain the
observations and source hashes. Each retained error log ends with the line
of the memory guard. That line gives the peak memory and the elapsed time
of the run.
