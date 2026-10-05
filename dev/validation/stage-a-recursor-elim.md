# A.5b.3.2e.15: checked recursor scrutinee and result type

This increment follows checked recursor branch coverage at `c65161e`.
`lib/kernel_recursor_elim.bend` checks the scrutinee of one structural
recursor and gives the result type and the usage of the elimination.
Ambient locals and captures, scrutinee types at instantiated parameters,
routing of the kernel `Elim` term to this entry, public elimination
hypothesis syntax, delayed recursive evaluation, erasure and families
without constructors remain pending. UAT.0 and Stage A acceptance stay open.

## Contract

`check` takes a context, a checking mode, a family name, a scrutinee
quantity, a scrutinee, a motive and a branch list. A branch is an address
and a leg. The steps follow the order of the kernel rule `mu_elim_elim`.

`check` first reads the family with the kernel rules. A refusal from these
rules is returned unchanged. `check` then makes the parameter context. This
is a fresh context that binds only the declaration parameters, each at its
declared quantity. The motive check and the body check use the same
context.

`check` infers the scrutinee in the parameter context with the kernel rule
`infer_scrutinee`. Each refusal of the rule is returned unchanged. An erased
scrutinee at a runtime mode gives a `Quantity` refusal. A scrutinee that
the kernel cannot type keeps its kernel error.

The type of the scrutinee must be a left former at a family. `check` uses
the kernel rules `with_former` and `as_mu` with the refusals of
`mu_elim_elim`. This step gives the weak head form of the type, the family
name and the index values.

The family name must be the family that the recursor eliminates. A
different name gives a `Mismatch` refusal that names both families.

The parameters of the scrutinee type must be the declaration parameters, in
order. `check` builds the former of the family at the declaration
parameters and the inferred index values. One kernel conversion compares
this former with the scrutinee type. A difference gives a `Not_yet`
refusal. The motive and the bodies are checked at the declaration
parameters, so `check` does not accept a scrutinee at other parameters.

`check` then calls the coverage check of
[A.5b.3.2e.14](stage-a-recursor-cover.md) with the same context, mode,
family, motive and branch list. Each refusal is returned unchanged: a
motive refusal, a family without constructors, a coverage refusal and a
body refusal. A scrutinee refusal thus comes before a coverage refusal.

The result type is the motive at the inferred index values and the
scrutinee value. `check` evaluates the scrutinee in the parameter context
and applies the kernel rule `mu_result`. The result usage is the scrutinee
usage in sequence with the branch usage scaled by the checking mode. The
kernel rule `mu_elim_elim` gives the same usage.

`check` does not capture ambient locals, instantiate parameters, bind
public elimination hypotheses, or evaluate a recursive call. The kernel
`Elim` term is not routed to this entry. The public compiler semantics
remain unchanged. No other kernel module changes.

## Design rulings

1. The steps have the order of `mu_elim_elim`: the scrutinee, the former,
   the branches, then the result.
2. The scrutinee scope is the declaration parameters only. It is the
   context of the motive and of the bodies.
3. The family name check and the parameter check run before the coverage
   check. A scrutinee refusal comes before a coverage or body refusal.
4. The parameter check is one conversion against the former of the family
   at the declaration parameters. The kernel conversion of two formers
   compares their parameters.
5. The result type uses the inferred index values and the scrutinee value.
   It does not use the declared indices.
6. The usage is a sequence, not a join. The scrutinee is used before the
   branches. The branch usage is scaled by the checking mode.
7. The refusals of the kernel rules and of the coverage check are returned
   unchanged.

## Validation scope

The dedicated harness uses freshly checked declarations and fixed expected
output. The driver binds an ambient local before it calls `check`, as the
cover driver does. The case `untypable` reads index 0 in a family without
parameters. Its refusal shows that the scrutinee does not see the ambient
local.

No public syntax exists for a scrutinee or a branch list. The driver takes
the family, a motive mode, a checking mode, a scrutinee quantity, a
scrutinee name and the branch groups. The scrutinee comes from a fixed
table of core terms. A scrutinee is a constructor term with a type
annotation or a parameter. One table entry is a constructor term without
an annotation. The groups have the form of the cover driver. An accepted
case prints the parameter count and the result type. The kernel printer
prints the type in the parameter context. For each parameter that has a
runtime read, the case also prints the name and the path interval.

The 24 accepted cases cover a zero and a successor scrutinee with a
constant motive, the linear mode, and a motive that depends on the
scrutinee: the printed type shows the scrutinee value. They cover a
parameterized list at its own parameter, an indexed family with a constant
motive and with an index motive, and a family with two erased parameters.
The printed type of the index case shows the inferred index. Three cases
use the erased checking mode, with an erased scrutinee, with a scrutinee of
quantity many and with a parameter read. Two cases use a family whose field
has the quantity many. Ten cases use a family with linear fields. A
parameter is read in the scrutinee only and in a branch only, each at four
pairs of checking mode and scrutinee quantity. A parameter is read in the
scrutinee and in a branch at two pairs. These cases show the sequence and
the scale of the usage.

The 19 refused cases cover a scrutinee that the kernel cannot type, a
constructor term without an annotation and a runtime read of an erased
parameter. They cover a scrutinee whose type is not a left former: a
parameter of a base type and a parameter that is a type. They cover a
scrutinee in another family, as a constructor term and as a parameter, and
a scrutinee at swapped parameters. An erased scrutinee is refused at the
many mode and at the linear mode. A missing branch after a good scrutinee
gives the coverage refusal. A scrutinee that cannot be typed with a missing
branch gives the scrutinee refusal. A scrutinee in another family with an
empty branch list gives the family refusal. The cases also cover the
propagated refusals: a wrong body, a wrong motive, a repeated branch and an
affine child. An undeclared family is refused. The driver refuses an
unknown motive mode, and a case checks that refusal.

The driver reads the family before it calls `check`. The refusal for an
undeclared family thus comes from the read in the driver. The harness has
no case for an ambient capture or for a scrutinee at instantiated
parameters that the kernel could accept. Both are excluded from this
increment.

`check` includes `test-recursor-elim`; `test` and `gates` include
`test-recursor-elim-mutations`. The full `make gates` suite is not claimed
for this increment. The existing M0 acceptance obligations remain required.

## Results

All 43 elimination cases pass on Bun and Node worker. Host runs perform
fresh Bend checks and compilation. The expected outputs were written from
the kernel source before the first run. The first run, on Bun, matched all
43. No expected output changed after a run. The run on Bun and Node worker
took 112 seconds and its peak memory was 1453 MB.

Ten compiled behavioral mutations cover a dropped family name check, a
dropped parameter check, coverage before the scrutinee, a dropped coverage
check, a result type at a fixed index, a result type at a fixed value in
place of the scrutinee value, a join in place of the sequence, a dropped
scale, a dropped scrutinee usage and a fixed scrutinee quantity. All ten
mutations are killed on Bun. Each killed mutant exited cleanly with output
that differs from the expected output. The mutation record names the case
that kills each mutant. This run took 159 seconds and its peak memory was
2702 MB.

The native host is not validated. A 4000 MB memory guard stopped a
native-only run at 4031 MB after 31 seconds. The run was not repeated. The
native log is retained beside this report. The default Make targets still
request all three hosts. A native pass remains open.

All 45 cover cases of A.5b.3.2e.14 pass again on Bun and Node worker. That
run took 22 seconds and its peak memory was 2602 MB.

The source policy and 26 Python tests pass: 61 scoped files, 49 line
budgets, 63 reviewed unsafe sites and 41 catchalls. This module adds no
unsafe or catch-all sites. The production driver passes
`dev/build.py --check`, and all 34 toolchain checks pass. The Makefile
change only appends the elimination targets. The house policy adds one line
budget for the new module. The module has 51 lines and its budget is 60.

The motive, branch, body, raw-layout and uniform-layout suites were not
rerun, because this increment changes no shared kernel module or existing
harness. The Bun and Node worker run used
`dev/test-recursor-elim.py --hosts bun,node-worker`. The mutation run used
`dev/test-recursor-elim.py --hosts bun --mutations`. The
[elimination record](stage-a-recursor-elim.json) and
[mutation record](stage-a-recursor-elim-mutations.json) retain the
observations and source hashes. Each retained error log ends with the line
of the memory guard. That line gives the peak memory and the elapsed time
of the run.
