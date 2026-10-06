# A.5b.3.2e.16: ambient locals and captures for the checked recursor

This increment follows the checked recursor scrutinee and result type at
`e52354f`. The checked recursor now runs under the given context. The
scrutinee, the motive and the branch bodies can read the given locals, and
the result usage reports their reads. Scrutinee types at instantiated
parameters, routing of the kernel `Elim` term to this entry, public
elimination hypothesis syntax, delayed recursive evaluation, erasure and
families without constructors remain pending. UAT.0 and Stage A acceptance
stay open.

## Contract

The scope of each recursor check is the given context, then the
declaration parameters. Each module extends the context that it receives
with the declaration parameters, each at its declared quantity. Before this
increment each module made a fresh context with the globals and the budget
of the given context, and the fresh context dropped the given locals.

Six scope sites change. They are the constructor layout in
`lib/kernel_recursor.bend`, the uniform validation in
`lib/kernel_recursor_uniform.bend`, the motive layout, the branch layout,
the body check and the elimination check. No other kernel line changes.

A term in the scrutinee, the motive or a body counts the leg binders
first, then the declaration parameters, then the given locals. For a
family without parameters the scope is the given context. This is the
scope of the kernel elimination rule. The parameters stay abstract
variables. A scrutinee type must still name the family at the declaration
parameters.

The uniform validation compares each parameter of a recursive child with
the variable of the parameter. The comparison now runs in the given
context, then the declaration prefix. The level of parameter `i` is the
size of the given context plus `i`. All other level sites already use the
size of the context they receive.

The result usage covers the given locals and the declaration parameters.
A read of a given local follows the rules of every kernel term. A runtime
read of an erased local gives a `Quantity` refusal. The usage of a linear
local is the interval of its reads: a sequence for the scrutinee and the
branches, a join across the branches. `check` does not close the given
locals. The binder that owns a local closes it, and the kernel binder check
refuses a linear local whose interval is not one to one.

With an empty given context the behavior does not change. `check` does not
instantiate parameters, bind public elimination hypotheses, or evaluate a
recursive call. The kernel `Elim` term is not routed to this entry. The
public compiler semantics remain unchanged.

## Design rulings

1. The scope order is the given locals, then the declaration parameters.
   The parameters are the innermost binders of the scope. A family without
   parameters is checked in the given context, as the kernel rule does.
2. All six modules extend the given context. No module makes a fresh
   context.
3. The parameters are not instantiated. An instantiated scrutinee type
   needs a parameter environment that is separate from the typing context.
   This remains pending.
4. The given locals are not closed by the recursor check. The owner of a
   local closes it. The usage of a local is reported as an interval.
5. The six old drivers now pass the plain context. Their expected outputs
   do not change. Their meaning changes from "the ambient local is ignored"
   to "no local is given".

## Validation scope

The dedicated harness uses freshly checked declarations and fixed expected
output. The driver is the elimination driver with one more argument. The
argument names a context from a fixed table. The table has an empty
context, one local `n : N` at the quantity many, zero and one, one local
`m : Nat` at the quantity many, zero and one, two locals `n` and `k`, one
local of an empty family, and an erased index with a vector at that index.
One table entry repeats the linear `m` with a close flag. For a closed
context the driver runs the kernel binder check on the result usage and
prints the result only if the check passes. This is the check that the
owner of the local does.

The driver binds the given locals in a fresh context and then calls
`check`. An accepted case prints the scope count and the result type. The
scope count is the number of given locals plus the number of declaration
parameters. For each scope variable that has a runtime read, the case also
prints the name and the path interval. The report lists the innermost
variable first. New scrutinees read a given local inside a constructor
term, and new bodies return a given local or a term that mentions one. A
new motive mode places the family `At` at the innermost given local.

The 43 elimination cases of A.5b.3.2e.15 run again at the empty context.
Their expected outputs do not change. The 26 new cases use a given local.

The 15 accepted cases cover a given local as the scrutinee of a family
without parameters, a motive that mentions a given local, and a body that
returns a given local. A linear local is read in every branch, so the join
gives the interval one to one, and the closed context accepts it. A linear
local is read in one branch only, so the interval is zero to one. A linear
local is read in the scrutinee and in a branch, so the sequence gives the
quantity many. A vector local is the scrutinee at an erased index that is
another local. A parameterized list with a recursive constructor is
eliminated under a given local, so every level shifts by one and the
uniform check passes. Two given locals are bound, and the outer local is
the scrutinee while a body reads the inner local. A self motive, an index
motive, a family with two erased parameters and a family with linear
fields are eliminated under a given local. An erased local is the
scrutinee at the erased checking mode.

The 11 refused cases cover the closed context for a linear local that is
read in one branch only and for a linear local that is read two times. An
erased local is refused as the scrutinee at the many mode and as a read in
a body at a runtime mode. The scrutinee quantity zero at the many mode is
refused under a given local. A local of a family without constructors
reaches the coverage refusal for that family. A local of
another family is refused with both family names. A scrutinee index and a
body index above the context are refused as unbound. A family with a
non-uniform recursive parameter is still refused under a given local. The
driver refuses an unknown context, and a case checks that refusal.

Six compiled behavioral mutations put one scope site back on a context
without the given locals. They cover the elimination check, the body check,
the motive layout, the branch layout, the constructor layout and the
uniform validation. Each mutation is killed by a case that reads a given
local at that site.

Two old mutation rows are removed. The layout row `ambient-context` and the
body row `ambient-origin` placed the scope on the given context, and that
is now the code. The body case `ambient-not-captured` stays, because with
no local given the index is still unbound. Two uniform mutation anchors
take the new base argument.

`check` includes `test-recursor-ambient`; `test` and `gates` include
`test-recursor-ambient-mutations`. The full `make gates` suite is not
claimed for this increment. The existing M0 acceptance obligations remain
required.

## Results

All 69 ambient cases pass on Bun and Node worker. Host runs perform fresh
Bend checks and compilation. The expected outputs were written from the
kernel source before the first run. The first run, on Bun, matched all 69.
No expected output changed after a run. One driver edit came before the
first run: a nested match on the context row did not compile, so the row
match moved to its own definition. The run on Bun and Node worker took 26
seconds and its peak memory was 2754 MB.

All six mutations are killed on Bun. Each killed mutant exited cleanly
with output that differs from the expected output. The mutation record
names the case that kills each mutant. This run took 106 seconds and its
peak memory was 2649 MB.

The native host is not validated. A 4000 MB memory guard stopped a
native-only run at 4372 MB after 11 seconds. The run was not repeated. The
native log is retained beside this report. The default Make targets still
request all three hosts. A native pass remains open.

The seven old recursor suites pass again on Bun and Node worker: 19 layout
cases in 16 seconds at 2225 MB, 34 uniform cases in 14 seconds at 2726 MB,
43 motive cases in 17 seconds at 2503 MB, 44 branch cases in 14 seconds at
2038 MB, 46 body cases in 19 seconds at 2413 MB, 45 cover cases in 16
seconds at 2660 MB and 43 elimination cases in 19 seconds at 2339 MB. No
expected output of an old suite changed.

The edited mutation suites pass again on Bun: 4 layout mutations in 45
seconds at 2519 MB, 6 uniform mutations in 80 seconds at 2441 MB and 8 body
mutations in 101 seconds at 2763 MB. The motive, branch, cover and
elimination mutation suites pass again on Bun: 8 motive mutations in 72
seconds at 2773 MB, 9 branch mutations in 77 seconds at 2796 MB, 9 cover
mutations in 78 seconds at 2773 MB and 10 elimination mutations in 81
seconds at 2807 MB. Every mutation is killed.

The source policy and 26 Python tests pass: 61 scoped files, 49 line
budgets, 63 reviewed unsafe sites and 41 catchalls. This increment adds no
kernel module, no unsafe site, no catch-all and no line budget. The
production driver passes `dev/build.py --check`, and all 34 toolchain
checks pass. The Makefile change only appends the ambient targets.

The Bun and Node worker run used
`dev/test-recursor-ambient.py --hosts bun,node-worker`. The mutation run
used `dev/test-recursor-ambient.py --hosts bun --mutations`. The
[ambient record](stage-a-recursor-ambient.json) and
[mutation record](stage-a-recursor-ambient-mutations.json) retain the
observations and source hashes. Each retained error log ends with the line
of the memory guard. That line gives the peak memory and the elapsed time
of the run.
