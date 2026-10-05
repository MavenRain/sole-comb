# A.5b.3.2e.14: checked recursor branch coverage

This increment follows checked recursor branch bodies at `52e3893`.
`lib/kernel_recursor_cover.bend` checks the full branch list of one
structural recursor against the constructors of its family. Ambient
captures, the scrutinee and the result type of the elimination, public
elimination hypotheses, delayed recursive evaluation, erasure and families
without constructors remain pending. UAT.0 and Stage A acceptance stay open.

## Contract

`check` takes a context, a checking mode, a family name, a motive and a
branch list. A branch is an address and a leg.

`check` first reads the family and its constructor names with the kernel
rules. A refusal from these rules is returned unchanged: an undeclared
family, a family that is still under declaration and a kernel family.

A family without constructors gets a `Not_yet` refusal. The motive check of
A.5b.3.2e.11 needs a constructor key. Without a constructor, no step would
check the motive.

`check` then applies the kernel coverage rule `mu_cover` before it checks
any body. Each refusal of the rule is returned unchanged. A branch address
that is not a constructor address gives a `Wrong_leg` refusal. A
constructor without a branch gives a `Missing_branch` refusal. A
constructor with two branches gives a `Wrong_leg` refusal. A branch key
that is not a constructor of the family gives an `Unbound` refusal. The
rule examines the constructors before the extra keys.

`check` then walks the constructors in declaration order. The branch order
has no effect. For each constructor, `check` takes the leg at the
constructor key and calls the body check of
[A.5b.3.2e.13](stage-a-recursor-body.md) with the same context, mode,
family and motive. The first refusal stops the walk and is returned
unchanged. This includes a motive refusal, a binder count or quantity
refusal, a body of the wrong type and the `Not_yet` refusal for an affine
recursive child.

The result is the join of the branch usages. The walk starts from the
unreachable usage and joins each branch usage with the kernel `alternative`
rule. The kernel rule for elimination branches uses the same fold. A
parameter that every branch uses one time is thus used one time. A
parameter that only some branches use has a path interval that starts at
zero.

`check` does not capture ambient locals, check a scrutinee, compute the
result type of the elimination, bind public elimination hypotheses, or
evaluate a recursive call. The public compiler semantics remain unchanged.
No other kernel module changes.

## Design rulings

1. Coverage is decided before any body. A coverage refusal comes before a
   body refusal.
2. The bodies are checked in constructor declaration order. The branch
   order is not used.
3. The leg lookup uses the constructor key form of the kernel branch rule.
4. The usage is the join over every branch. The base is the unreachable
   usage.
5. A family without constructors is refused, because its motive would stay
   unchecked.
6. The family and coverage refusals of the kernel rules are returned
   unchanged.

## Validation scope

The dedicated harness uses freshly checked declarations and fixed expected
output. The driver binds an ambient local before it calls `check`, as the
body driver does.

No public syntax exists for a branch list. The driver takes the family, a
motive mode, a checking mode and the branch groups. A group is a key, a
body from the fixed table of core terms, then the leg binders as quantity
and name pairs. A `/` argument closes a group. The key `@leg` gives an
address that is not a constructor address. An accepted case prints the
parameter count. For each parameter that has a runtime read after the join,
it also prints the name and the path interval.

Accepted cases cover a family with two constructors in both branch orders,
a dependent motive, a parameterized list, an indexed family with a constant
motive and with an index motive, a family with three constructors and two
recursive children in two branch orders, a zero child, two families with
one constructor and the erased checking mode. Six cases show the join: a
parameter used in every branch, in the first branch only, in the last
branch only and in no branch, then the every-branch case at the linear mode
and at the erased mode.

Refused cases cover a missing first, last and middle branch, an empty
branch list, a repeated first and last branch, a key that is not a
constructor with and without complete coverage, and an address that is not
a constructor address. They cover a wrong body in the first branch, in the
last branch only, in a middle branch and in both branches. With two wrong
bodies, both branch orders give the refusal of the first constructor in
declaration order. A wrong body with a missing branch, a wrong body with an
extra key and a wrong motive with a missing branch each give the coverage
refusal. The cases also cover the propagated body refusals: a binder count
on a direct and on a nullary constructor, a field and a hypothesis
quantity, an erased hypothesis read at runtime, an affine child and a wrong
motive. An undeclared family and a family without constructors are refused.
The driver refuses an unknown motive mode, and a case checks that refusal.

The driver reads the family before it calls `check`. The refusal for an
undeclared family thus comes from the read in the driver. The harness has
no case for a family under declaration or for a kernel family.

`check` includes `test-recursor-cover`; `test` and `gates` include
`test-recursor-cover-mutations`. The full `make gates` suite is not claimed
for this increment. The existing M0 acceptance obligations remain required.

## Results

All 45 cover cases pass on Bun and Node worker. Host runs perform fresh Bend
checks and compilation. The expected outputs were written from the kernel
source before the first run. The first run that reached the cases matched
all 45. This run took 14 seconds and its peak memory was 2752 MB.

Nine compiled behavioral mutations cover a dropped coverage rule, coverage
after the bodies, a walk in branch order, a walk that stops after the first
constructor, one fixed key for every leg lookup, a sequential sum in place
of the join, an empty base usage, a dropped guard for a family without
constructors and bodies checked in the erased mode. All nine mutations are
killed on Bun. Each killed mutant exited cleanly with output that differs
from the expected output. This run took 71 seconds and its peak memory was
2744 MB.

The native host is not validated. A 4000 MB memory guard stopped a
native-only run at 4952 MB after 10 seconds. The run was not repeated. The
native log is retained beside this report. The default Make targets still
request all three hosts. A native pass remains open.

All 46 body cases of A.5b.3.2e.13 pass again on Bun and Node worker. That
run took 14 seconds and its peak memory was 2736 MB.

The source policy and 26 Python tests pass: 60 scoped files, 48 line
budgets, 63 reviewed unsafe sites and 41 catchalls. This module adds no
unsafe or catch-all sites. The production driver passes
`dev/build.py --check`, and all 34 toolchain checks pass. The Makefile
change only appends the cover targets. The house policy adds one line budget
for the new module.

The motive, branch, raw-layout and uniform-layout suites were not rerun,
because this increment changes no shared kernel module or existing harness.
The Bun and Node worker run used
`dev/test-recursor-cover.py --hosts bun,node-worker`. The mutation run used
`dev/test-recursor-cover.py --hosts bun --mutations`. The
[cover record](stage-a-recursor-cover.json) and
[mutation record](stage-a-recursor-cover-mutations.json) retain the
observations and source hashes. Each retained error log ends with the line
of the memory guard. That line gives the peak memory and the elapsed time
of the run.
