# A.5b.3.2e.17: checked recursor scrutinee at instantiated parameters

This increment follows the ambient locals and captures of the checked
recursor at `c447d49`. The checked recursor now infers the scrutinee in the
given context, and the scrutinee type gives the declaration parameters
their values. The motive, the branch bodies, the field types and the
uniform validation see those values. Routing of the kernel `Elim` term to
this entry, parameter rows in the result usage, public elimination
hypothesis syntax, delayed recursive evaluation, erasure and families
without constructors remain pending. UAT.0 and Stage A acceptance stay
open.

## Contract

The scrutinee is inferred in the given context. Its term indexes the given
locals only. Before this increment the entry opened the declaration
parameters as abstract locals after the given locals, inferred the
scrutinee in that origin, and refused with `Not_yet` every scrutinee whose
parameters were not those abstract locals. That check and its text are
removed. Instantiation replaces it.

The scrutinee type must still be a left former at the family of the
recursor. The view of the type keeps the family name, the index values and
the former closure. The parameter legs of the closure open into the
parameter values with the kernel rule `mu_param_env`. That rule returns
the values with the last parameter first. The entry reverses the list one
time into telescope order.

A scope value selects how the declaration parameters open. `Abstract`
binds each parameter at its declared quantity, as before. `Instance`
carries the values in telescope order. It walks the telescope and the
values together, evaluates each parameter type in the current environment
and defines the parameter at quantity zero with its value. A length
mismatch between the telescope and the values is a `Mismatch` refusal with
the kernel pack text. A defined parameter is an erased alias: every
evaluation through the environment sees the value, and a body that reads
the parameter slot at a runtime mode is refused by the erased read rule.

Every module entry that opens the declaration parameters takes the scope
and passes it down: the constructor layout, the uniform validation, the
motive check, the branch check, the body check and the coverage check. The
elimination check builds the instance scope from the scrutinee type, opens
the parameters, and runs the coverage check after the family name check.
The old drivers pass the abstract scope, so their expected outputs stay
the same except for the rows listed under the scope change below.

The uniform validation compares the parameter `i` of a recursive child
with the expected value of parameter `i`. The abstract scope expects the
variable at the size of the given context plus `i`, as before. The
instance scope expects the value `i`. The walk builds the expected list
one time per layout and walks the child parameters and the expected list
together. At an instance the recursive field type evaluates through the
defined slots, so `List N` at parameter `N` compares `N` with `N`.

The scrutinee value is the evaluation of the term in the given context.
The result type is the kernel result rule in the opened context at the
inferred indices and that value. Values are level based, so a value over
the given context is valid in the opened context. The result usage is the
scrutinee usage in sequence with the scaled branch usage. The usage rows
are keyed by the level of the local, so the scrutinee usage over the given
context and the branch usage over the opened context align on the given
locals without padding. The parameter rows always show zero use and stay
in the result usage in this increment.

The kernel checks a parameter leg against `Type 1`, the universe of the
parameter domain. A given local bound at `Type 0` is refused as a
parameter value with a universe mismatch. The suite records this as an
observation at three rows and binds the type locals of the other rows at
`Type 1`. Whether the leg check must accept `Type 0` stays open.

## Design rulings

1. The scrutinee is inferred in the given context, as the kernel
   elimination rule does. The declaration parameters are not in scope for
   the scrutinee term.
2. The parameter values come from the scrutinee type through the kernel
   rule `mu_param_env` and are reversed one time into telescope order.
3. An instance parameter is defined at quantity zero. The parameters are
   not hypotheses of the motive or the bodies; a parameter slot is a type
   level view of the scrutinee's parameters.
4. The scope value is threaded through every module that opens the
   parameters. No module makes its own scope.
5. The usage rows align by level. No padding row is added.
6. The old drivers pass the abstract scope. Their meaning does not change.

## Scope change of the elimination and ambient rows

The elimination suite runs at the empty context. Every scrutinee entry
whose annotation or term pointed at a declaration parameter is now unbound
in the given context. 20 of the 43 elimination rows change: the rows at
the parameterized families `List`, `P`, `U` and `L` now expect the unbound
index of the first parameter they reach, and the row that refused "other
parameters" with `Not_yet` now expects an unbound index because that text
is removed. Every row at `N`, `Out`, `V`, `E` and `T` keeps its
expectation. The 20 `none` copies in the ambient suite change with them.

Four ambient rows under a given local change. Two rows that reached a
parameter past the local are unbound. Two rows that gave the local `n : N`
as the parameter value are refused with the universe mismatch. All 24
changed expectations were written down from the source before the first
run. The two universe rows were predicted with `Type 0` and observed with
`Type 1`; the observed text was adopted, and the universe observation
above records the reason.

The elimination mutation suite loses the mutant anchored on the removed
parameter check and the three usage mutants whose witnesses needed a given
local. Those three move to the instance suite. The elimination suite keeps
6 mutants. The ambient suite keeps 6 mutants: the uniform scope mutant
moves to the instance suite, and a coverage scope mutant takes its place
at the new coverage call. Six stale anchors in the layout, uniform and
coverage mutation suites were re-anchored on the new signatures.

## Validation scope

The dedicated harness uses freshly checked declarations and fixed expected
output. The driver is the ambient driver with a larger scrutinee table and
a larger context table. The scrutinee table adds `nil` and `cons` at
`List N`, `nil` at `List Nat`, `p` at `P N N` and at `P A N` for a given
type local `A`, `keep` at `U m m` and `hold` at `L m m` for a given local
`m`, and `leaf` at `T N` and at `T Nat`. The context table keeps the
ambient contexts and adds a type local `A` at `Type 0` and at `Type 1`,
the same with a local `n : N` before it, a list local `xs : List A` at the
quantities one and zero, a list local `xs : List N` alone and after a
local `n : N`, and two locals `m` and `k` at `Nat`. No vector family
exists in the prelude, so no context binds a vector at a local length.

An accepted case prints the scope count and the result type. The scope
count is the number of given locals plus the number of declaration
parameters. The result type is printed in a context that opens the
parameters by name only, so an instance prints the value and never a
parameter name. For each scope variable with a runtime read, the case also
prints the name and the path interval, innermost first. The motive modes
`parameter` and `earlier-parameter` place a parameter slot at a type
position, so at an instance they print the value.

The 23 accepted cases cover a closed instance at the empty context for
`nil` and `cons` at `List N` and `nil` at `List Nat`, at the quantities
many, one and zero. A two parameter family is eliminated at two closed
values. The `parameter` motive shows `N` as the result type at `List N`,
and a body returns the head field, whose type is the instantiated `N`. The
second parameter of `P A N` shows `N` under a type local `A` and a local
`n`. A list local at `List N` is the scrutinee at the quantities one and
many, and its usage row shows one to one or many. A body returns the head
of a list local, with the usage rows of the list and of an earlier local.
A list local at `List A` with `A : Type 1` is the scrutinee at the
quantities one and zero. A family with two erased parameters is eliminated
at `U m m`, a body reads the local `m`, and a body reads the field. A
parameter slot is read at the erased mode. A linear family is eliminated
at `L m m` with a linear local read one time, also in the closed context,
and with two locals `m` and `k` read one time each. A tree family with a
recursive constructor is eliminated at `T Nat`, so the uniform check
compares `Nat` with `Nat`, and the same list family under a type local.

The 25 refused cases cover the three universe observation rows. The
`earlier-parameter` motive at `P A N` shows `A` where `N` is asked. A body
returns the head of a list local at `List A` where `N` is asked. A list
local at quantity zero is read at a runtime mode. A scrutinee quantity
zero at the many mode is refused under a list local. A body reads a
parameter slot at a runtime mode, for the first and the second parameter
of `U` and for a parameter of `L`. A linear local read in the scrutinee
and in a body is refused by the closed check. A non uniform recursive
parameter is still refused with `Not_yet` at a closed instance and under a
type local. A body returns the head `N` or `Nat` where `Out` is asked, and
a body returns the tail `List N` where `N` is asked, so the field types are
instantiated. A missing branch at an instance is refused. A scrutinee of
another family is refused by the name check. A type local as the scrutinee
is not a left former, and a type local read at a runtime mode is erased. A
local `n : N` as the value of a type parameter is refused with the
universe mismatch, also with the two legs of `P` swapped. A scrutinee that
indexes past the given context is unbound. The driver refuses an unknown
context and an unknown scrutinee, and a case checks each refusal.

Eleven compiled behavioral mutations break one instance site each. Two
remove the diagram-width and parameter-binder checks added by review.
The original nine exercise the following sites. The
harness names the file of each mutant. In the elimination module: the
instance scope replaced by the abstract scope, the value list not reversed,
the scrutinee usage dropped, the branch usage dropped, and the branch
usage scaled by many. In the recursor module: the instance parameter
defined at quantity many, and the instance arm of `open` replaced by the
abstract walk. In the uniform module: the instance arm of the expected
list replaced by the levels, and the comparison arms swapped. Each
mutation is killed by a case that sees the value at that site. A mutant
that drops the given locals from the uniform validation is not killable
with this driver, because the uniform refusal prints no value; the two
uniform mutants above take its place.

`check` includes `test-recursor-instance`; `test` and `gates` include
`test-recursor-instance-mutations`. The full `make gates` suite is not
claimed for this increment. The existing M0 acceptance obligations remain
required.

## Results

The final review runs 53 instance cases on Bun and Node worker and kills
11 instance mutations on Bun. The original 48 cases retain their expected
outputs. The five added cases cover a valid parameterless diagram, wrong
widths at parameterless and instantiated families, a noncollection
diagram, and a parameter leg with a binder. Two added mutations remove
the width and binder checks independently.

The review found that extracting parameter values discarded diagram
structure formerly checked by conversion. A parameterless `N.zero`
annotated with `SColl 1` and no legs was rejected at the base revision but
accepted by the staged implementation. An instantiated `List N` diagram
also accepted a parameter leg with an extra binder. The new
`kernel_recursor_instance.bend` helper requires a collection, the declared
parameter width, the same number of legs, and no leg binders before
extracting values. These witnesses use raw `T.Ann` terms through the
tested `Elim.check` API; parsed surface syntax reachability was not
established.

The eight existing recursor suites are rerun on Bun and Node worker:
19 layout, 34 uniform, 43 motive, 44 branch, 46 body, 45 cover,
43 elimination and 69 ambient cases. The four layout mutations are also
rerun because the existing family-group traversal moved into
`kernel_recursor_group.bend`. The original 130-line recursor limit is
preserved; the new group and instance helpers have limits of 40 and
35 lines. Source policy covers 63 files and 51 line budgets, with the
same 63 reviewed unsafe sites and 41 catchalls. Policy fixtures, the
production build check and all 34 toolchain checks are included.

The new default Make targets explicitly run Bun and Node worker.
`make test-recursor-instance-native` retains an explicit native check.
The pre-review native compile did not finish within the unchanged
900-second harness deadline. Its retained native-cap log records 922
seconds and a 3967 MB peak under the 4000 MB guard. Native has not been
revalidated and no native pass or full `make gates` pass is claimed.

The [instance record](stage-a-recursor-instance.json) and
[mutation record](stage-a-recursor-instance-mutations.json) are regenerated
from the final reviewed sources. Both records include the 53 cases on
Bun and Node worker; the mutation record also includes the 11 killed
mutations. The [review record](stage-a-recursor-instance-review.json)
retains exact commands, results, stdout, stderr, elapsed time and source
hashes for the final checks, together with the before-fix witnesses.
Other retained stdout and stderr logs describe the original pre-review
runs, including the eight original mutation-suite runs and the native
timeout. Their memory-guard measurements are historical, not measurements
of the final review runs. The existing M0 acceptance obligations remain
required.
