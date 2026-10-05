# Stage A compiler milestones

The [public compiler entry decision](public-compiler-entry.md), adopted on
2026-09-29, moves the real public driver and supported `.sole-comb` examples
ahead of the former Stage C frontend timing. Public source checking now
accompanies kernel work. The original complete semantic gates remain required.

Adopted on 2026-09-27. This replaces the whole-Stage-A commit boundary in
M0-PLAN section 11 and the matching user-commit schedule in section 12.
The pinned Kanon semantics, original
Stage A implementation units, and complete integration gate remain in force.
Stages B through F retain their existing stage boundaries.

## Entry

The user's subsequent [entry decision](stage-a-entry.md) authorizes Stage A
now in the real compiler repository. Scratch feature development is retired.
R2 stays pending and gates the operational compiler before M0 closure.
This replaces the earlier scratch-first entry condition.

A.1 foundation, A.2 representation, A.3 evaluation and conversion, A.4
checking, A.5a test frontend parsing, A.5b.1 expression/declaration
elaboration, and A.5b.2 recursive programs are implemented. Their scoped checks and representation choices
are recorded in
[A.1 validation](validation/stage-a-foundation.md),
[A.2 validation](validation/stage-a-representation.md),
[A.3 validation](validation/stage-a-evaluation.md),
[A.4 validation](validation/stage-a-checking.md),
[A.5a validation](validation/stage-a-pinfront.md),
[A.5b.1 validation](validation/stage-a-elaboration.md), and
[A.5b.2 validation](validation/stage-a-program.md).
A.5b.3.1 adds the erased representation, printer, and runtime scope operations,
with [erasure support validation](validation/stage-a-erasure.md).
The public source-checking increment adds `bin/sole-comb.bend`, production
`surface/` components, and a public-command suite over native examples and
refusals. Next work expands that public compiler and completes A.5b.3.2
type-directed erasure and full oracle integration before `build` and `run`.
The function-arm increment extends native finite-sum elimination with named
and local function values, partial applications, and multi-binder lambdas.
Its public command suite includes domain, result, nonfunction, and quantity
refusals. See [function-arm validation](validation/public-function-arms.md).
The default-arm increment adds final `else: term` sugar through
`surface/sugar.bend`. Its public suite checks explicit kernel expansion equality
and syntax, payload, result, and quantity refusals. See
[default-arm validation](validation/public-default-arms.md).
The closed-record increment adds product declarations and qualified field
getters, with [record validation](validation/public-records.md). It preserves
the pending type-directed erasure and complete oracle integration boundary.
The A.5b.3.2a increment adds ordinary type-directed erasure to `check --erased`,
with [core erasure validation](validation/stage-a-core-erasure.md). The
A.5b.3.2b increment extends it to collection tuples and closed records, with
[product erasure validation](validation/stage-a-product-erasure.md). The
A.5b.3.2c increment extends it to finite-sum layouts, injections and case
elimination, with [sum erasure validation](validation/stage-a-sum-erasure.md).
The A.5b.3.2d increment adds dependent-pair layouts, introductions and native
two-binder elimination, with [pair erasure validation](validation/stage-a-pair-erasure.md).
The full A.5b.3.2 boundary remains pending for recursive definitions,
structural recursor sugar, unconstrained parameterized-constructor
inference, family default arms, complete corpus integration and
WebAssembly build/run.
The A.5b.3.2e.1 increment adds nominal recursive-family representations and
constructor layout planning in `erase/family.bend`. It resets the local scope,
opens parameters as variables, evaluates dependent field types, and omits
zero-quantity, type and proof fields. Constructor tags follow the completed
family's declaration order. `make test-family-layout` compares these plans
with pinned Kanon reference fixtures on Bun, Node worker and native;
`make test-family-layout-mutations` also checks four semantic mutations on Bun.
The [layout evidence](validation/stage-a-family-layout.md) describes the
record, which contains exact observations and source hashes.
The A.5b.3.2e.2 increment adds runtime constructor and match erasure in
`erase/mu.bend` and `erase/core.bend`. The test frontend compares six family
fixtures with recorded pinned Kanon CLI observations on Bun, Node worker and
native. The first native comparison timed out at the tracked 900-second limit and passed with a local 2400-second compile deadline. The review rerun passed with the tracked limits.
Twenty-eight independent goldens, each keyed to its own fixture, cover
constructor order, omitted fields, generic placeholders, dependent indices,
runtime parameters, branch arities and family groups. Eight isolated semantic
mutations exercise tags, ghost fields, binder order, constructor groups,
lifted-closure groups and tail flags. See [family erasure validation](validation/stage-a-family-erasure.md).
A.5b.3.2e.3 admits public brace-form families and positional constructor arms.
The native source checker retains checked family metadata for erasure.
Public examples cover mutual and indexed families, parameters, empty matches,
dependent generic slots and function-valued branch results. The public-family
harness compares their erased output with a frozen pinned Kanon expansion. It
also checks 16 literal goldens, a literal order probe and four mutations.
The native public-family check passed. The full `make gates` has not been
rerun with the public-family targets.
Recursive definitions, structural recursor sugar, unconstrained
parameterized-constructor inference, family default arms, complete
erased-corpus integration, and WebAssembly build/run remain pending. See the
[public family validation](validation/stage-a-public-family.md).

A.5b.3.2e.4 delivers expected-type constructor parameter inference through the
public compiler. Fields elaborate against the instantiated family telescope.
Constructors of families without parameters also give their field types to
their arguments. The elaborator does not run a kernel check for each field.
The kernel declaration check validates parameters, fields, indices and usage
once, so a nested constructor no longer runs a kernel check at each level.
[Constructor parameter validation](validation/stage-a-constructor-parameters.md)
records the Bun and Node worker checks for that increment. Unconstrained
constructor inference remains pending.
A.5b.3.2e.5 extends the telescope walker to family parameters and indices.
Each family argument receives its instantiated expected type. Parameter values
remain in scope when the walker reaches the indices. The public example and
56-case constructor suite cover this behavior, including nested and nullary
constructors, aliases, open variables, dependent arguments and refusals. See
[family argument validation](validation/stage-a-family-arguments.md).
A.5b.3.2e.6 adds constructor parameter inference from field types when the
constructor has no expected family. Direct parameter occurrences and nominal
family parameters and indices provide constraints. Later fields receive their
instantiated expected types. The result carries a kernel-checked family
annotation, so a constructor can be an unannotated elimination scrutinee.
The [constructor inference example](../examples/constructor-inference.sole-comb)
covers nested constructors, distinct and open parameters, recursive tails and
value parameters inferred from a nominal index. See
[constructor inference validation](validation/stage-a-constructor-inference.md).

Inference reads the fields from left to right. It first puts the solved
parameters and the earlier field values into the field type. If no unsolved
parameter remains in that type, the checker checks the argument against it. If
an unsolved parameter remains in any position, the checker does not use the
field type as an expected type. It infers the type of the argument and solves
the parameters from that type. A parameter gets a value at the top of the field
type and in the parameters and indices of a nominal family. A parameter beneath
a function, product or sum former gets no value from that field. Another field
must supply it. The kernel then checks the annotated constructor with all its
fields. An argument without a type of its own, such as a lambda or a tuple
without an annotation, cannot be inferred. In a field with an unsolved
parameter, the checker refuses it with a text that names the constructor and the
field. A nullary constructor of a family with parameters, such as `nil`, is
refused in the same way. For `r : (Nat -> A) -> A -> R A`,
`r (fun (n : Nat) => n) 7` is refused. For `r2 : A -> (Nat -> A) -> R2 A`,
`r2 7 (fun (n : Nat) => n)` is accepted. For `w3 : Both (Nat -> A) A -> W3 A`
and `g4 : Nat -> Nat`, `w3 (both g4 7)` is accepted. The second parameter of
`Both` gives the value of A. Nullary and phantom parameters without field
evidence remain refused.

This increment started the UAT.0 constructor requirement probes. The subsequent
readiness increment pins the UAT revision and dependencies, selected declaration
types and transitive axiom inventory, with public-source requirement probes.
See [UAT.0 readiness validation](validation/uat-readiness.md).

A.5b.3.2e.9 adds the direct recursive-field layout prerequisite for structural
recursors. It opens parameters in a fresh context, normalizes field domains,
and records direct self-recursive fields with their quantities and indices.
It refuses non-direct self occurrences and fields that mention another member
of a mutual group. The layout is metadata, not a checked recursor certificate.
Public branch elaboration, recursor typing, delayed IH evaluation and erasure
remain pending, so UAT.0 stays open. See
[recursor layout validation](validation/stage-a-recursor-layout.md).

A.5b.3.2e.10 checks uniform parameters for direct recursive-field layouts in
`lib/kernel_recursor_uniform.bend`. It compares child arguments in declaration
order with typed conversion, evaluates dependent parameter domains in their
declaration prefix, and uses the complete field context for comparison.
Aliases and proof-irrelevant arguments are accepted. Changed type or value
parameters, including those of zero-quantity children, are refused. This
checked metadata is another structural recursor prerequisite; public branch
typing, delayed induction hypotheses and erasure remain pending. See
[uniform layout validation](validation/stage-a-recursor-uniform-layout.md).

A.5b.3.2e.11 checks recursor motives and constructs the type of each recursive
field's induction hypothesis in `lib/kernel_recursor_motive.bend`. It reuses
uniform layout validation, checks the family and index binders, and preserves
the existing proposition elimination restriction. Motives are scoped under
declaration parameters and applied to each child's indices and value. The
returned plan keeps hypothesis types separate from constructor fields. Public
branch binders, delayed recursive evaluation and erasure remain pending. See
[motive validation](validation/stage-a-recursor-motive.md).

A.5b.3.2e.12 checks recursor branch types in
`lib/kernel_recursor_branch.bend`. It reuses the checked motive plan and
binds the constructor fields in declaration order, then one hypothesis for
each direct recursive child. A zero child gives a zero hypothesis and a many
child gives a many hypothesis. The planner refuses an affine child. The
target is the motive at the constructor indices and value. The kernel
checks each domain in its prefix context and the target after all binders.
Branch bodies, public elimination hypotheses, delayed recursive evaluation
and erasure remain pending. See
[branch validation](validation/stage-a-recursor-branch.md).

A.5b.3.2e.13 checks recursor branch bodies in
`lib/kernel_recursor_body.bend`. It takes the branch plan of A.5b.3.2e.12 and
one leg. The leg must bind every field and every hypothesis, and each leg
quantity must agree with the plan. The scope is the declaration parameters,
then the leg binders with the plan quantities and domains. The kernel checks
the body against the plan target at the runtime mode and closes the usage
back to the parameters. Constructor coverage, ambient captures, public
elimination hypotheses, delayed recursive evaluation and erasure remain
pending. See [body validation](validation/stage-a-recursor-body.md).

A.5b.3.2e.14 checks recursor branch coverage in
`lib/kernel_recursor_cover.bend`. It takes one family, one motive and the
full branch list. A family without constructors is refused. The kernel
coverage rule runs before any body: each constructor has one branch, and
each branch key is a constructor of the family. The body check of
A.5b.3.2e.13 then runs for each constructor in declaration order. The first
refusal stops the walk. The result is the join of the branch usages. Ambient
captures, the scrutinee and the result type of the elimination, public
elimination hypotheses, delayed recursive evaluation, erasure and families
without constructors remain pending. See
[cover validation](validation/stage-a-recursor-cover.md).

A.5b.3.2e.15 checks the recursor scrutinee and the result type in
`lib/kernel_recursor_elim.bend`. It takes one family, a scrutinee with its
quantity, one motive and the full branch list. The steps follow the kernel
elimination rule. The scrutinee is inferred in the scope of the declaration
parameters. Its type must be a left former at the eliminated family, and its
parameters must be the declaration parameters. The coverage check of
A.5b.3.2e.14 then runs. The result type is the motive at the inferred
indices and the scrutinee value. The result usage is the scrutinee usage in
sequence with the scaled branch usage. Ambient locals and captures,
scrutinee types at instantiated parameters, routing of the kernel `Elim`
term to this entry, public elimination hypothesis syntax, delayed recursive
evaluation, erasure and families without constructors remain pending. See
[elimination validation](validation/stage-a-recursor-elim.md).

A.5b.3.2e.7 adds final default arms for public families. Expansion tracks the
constructors already handled by explicit arms and generates checked arms for
the remainder in declaration order. The public examples cover field quantities,
dependent fields, parameterized and indexed families, captures and function
results. Checked and erased output agrees with independent explicit expansions
on all three hosts. See [family default validation](validation/stage-a-family-defaults.md).

Recursive definitions, structural recursor sugar, complete erased-corpus
integration, WebAssembly build/run and the full kernel differential remain
pending.
The constructor inference validation records the current host checks and
integration gate status.

Semantic gates compare with recorded Kanon fixtures, not with live OCaml
adapters. The [no-OCaml cleanup evidence](validation/no-ocaml-cleanup.md)
records the provenance of each fixture.

## Compiler commits

| Milestone | Existing units and tracked source | Required local evidence |
|---|---|---|
| A.1, foundation | A1: `lib/foundation.bend`, `kernel_budget.bend`, `kernel_error.bend`, `kernel_level.bend`, `kernel_literal.bend` | Pinned host checks/builds for the landed modules, source-policy checks, and focused foundation, budget, error, level, and literal checks. |
| A.2, representation | A2: `kernel_term.bend`, `kernel_value.bend`, `kernel_shape.bend`, `kernel_global.bend`, `kernel_prim.bend`, plus the A.2 subsets of `kernel_quantity.bend` (tags), `kernel_positivity.bend` (records and constructor lookup), and `kernel_rules.bend` (arrow, unit, and boolean builders) | Build the landed dependency closure and exercise representation, lookup, and primitive contracts. |
| A.3, evaluation | A3: `kernel_eval.bend`, `kernel_conv.bend`, the runtime and conversion subset of `kernel_rules.bend`, quantity equality, and signed host index arithmetic | Evaluation and conversion cases covering normal forms, refusal and budget exhaustion, plus comparisons with pinned Kanon reference fixtures. |
| A.4, checking | A4 through A6: the remaining rules, checking, positivity checking, the quantity algebra, printing, and specification counts | Focused acceptance/refusal, typing, shape, positivity, quantity, and printing checks, the specification counts, and earlier milestone regressions. |
| A.5a, test frontend parsing | A7 corpus and gate plumbing; A8 lexer, parser, and syntax printing in `test/pinfront/` | Every pinned corpus file and supplemental lexer/parser case compared with immutable pinned Kanon fixtures on Bun, Node worker, and native; malformed-output and isolated mutation checks. |
| A.5b.1, elaboration entry points | A9 through A10 expression and declaration elaboration in `test/pinfront/`, ordinary declaration checking, and mutual family admission | Exact pinned declaration observations for all 146 corpus files and focused probes on Bun, Node worker, and native; isolated semantic mutations. Recursive groups exercise the declaration API refusal. |
| A.5b.2, recursive programs | A9 through A10 recursive program driver, order certificates, totality checking, and A7 check/print observation plumbing | Exact stdout, stderr, and exit observations for all 146 corpus files and focused probes on Bun, Node worker, and native; direct contracts and isolated mutations. |
| A.5b.3.1, erasure support | Pinned `lib/eterm.ml` representation and printer in `erase/eterm.bend`; runtime variable reindexing, collection, and capture pruning from `lib/erase.ml` in `erase/runtime.bend` | Exact pinned observations on Bun, Node worker, and native, independent binding and printer expectations, and isolated semantic mutations. |
| A.5b.3.2, full oracle integration | Remaining A7 plumbing, the type-directed erasure pass, and complete frontend integration | Reproducible builds and a runnable kernel differential harness over the complete pinned corpus and all required modes. |
| A.close, Stage A acceptance | A11 through A13: full verification, fixes, and review close | Every Stage A gate and mutant check, complete KANON-DIFF under the existing divergence policy, HOUSE, and BUILD-TIME. R2-RISK-A moves to the operational-compiler gate under the entry decision. |

Required build and test plumbing may accompany the first milestone that
needs it, even when its original unit is A7. Record that dependency move;
it does not remove A7's remaining deliverables or change any gate.
A.5 is split at the parser boundary so its source and comparison harness can
be reviewed before elaboration. A.5a retains all 146 pinned inputs and marks
R4 survivor decisions pending. It does not complete KANON-DIFF, the check,
print, or erased-mode kernel comparisons, or Stage A acceptance.

A.5b.1 separates the expression/declaration entry points from the recursive
program driver. It ports family and constructor elaboration, dependent
motives, and field annotations. The reference invokes those same entry
points in pinned Kanon, so recursive function groups produce their explicit
declaration refusal. Totality/order translation, whole-program integration,
CLI output/exit modes, and the full KANON-DIFF follow that increment.

A.5b.2 ports the recursive driver and its order/totality dependencies, then
compares whole-program check and print observations. The pinned CLI also
supports `check --erased` (bin/kanon.ml:61-74), which needs the 1,484-line
lib/erase.ml dependency. A.5b.3.1 ports its runtime scope helpers and the
erased representation as a bounded prerequisite. A.5b.3.2 retains the
type-directed pass and full mode integration as the next review boundary.
No corpus file or required mode is removed from Stage A acceptance.

Each milestone must build its landed dependency closure, pass the applicable
source policies and focused behavioral checks, and receive a bounded review
before it is staged for the user. Preserve the checks and their commands in
`dev/validation/`. Record full-stage checks as pending until they can run.
Do not label a local check as a complete oracle differential or Stage A pass.

Agents stage only the reviewed milestone and provide its commit message and
command. The user commits. Source commits may land before A.close, but Stage
A stays incomplete until A.close passes. Stage B waits for that complete
gate. Later findings are fixed in subsequent milestones and rerun the
affected earlier checks.

## UAT portability priority

Added on 2026-10-02 at the user's request. A.5b.3.2e.6 starts the UAT.0
requirement probes with constructor inference. Structural recursors remain
planned.
Public structural recursion uses `elim`; `def rec` stays excluded.

The [UAT proof portability schedule](uat-proof-portability.md) sets the first
M1 implementation sequence:

- equality in `Prop` and transport;
- dependent records;
- universe polymorphism and instance resolution;
- the equality/categorical library;
- a checked UAT pilot.

These additions precede new seed breadth, lex2 and conditional brec2 work.
Existing M1 acceptance obligations remain required.

Preparation begins now. Compatibility changes to the pinned semantics begin
after M0 acceptance under an explicit M1 semantic-delta gate. The existing
Stage A and M0 corpus, modes, divergence policy and closure checks remain
required. The pilot and the full UAT port have separate completion criteria
in that schedule.

The UAT.0 readiness increment records 21 selected Lean declarations at revision
`f9d2bc270631eaefb9985c38e0c804b825a7ee2d`, all ten locked dependencies and
twelve public-source probes. `make test-uat-readiness` checks the immutable
baseline and current source boundary without requiring Lean for routine gates.
Structural recursors remain the next Stage A prerequisite. The UAT.0 row stays
open until they land. The M1 implementations remain pending.

## Integration gate preservation

Retain the full pinned corpus, output and exit comparisons, divergence
policy, source policies, build-time check, mutants, fix round, and
final review required by M0-PLAN. Early source commits do not shrink the
corpus, waive an unrun check, introduce a semantic delta, or satisfy the
stage gate. R2 remains required under the entry decision's revised timing.
The production kernel follows the pinned-port plan; scratch
findings and test cases inform its implementation.
