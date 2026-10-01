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
with [core erasure validation](validation/stage-a-core-erasure.md). The full
A.5b.3.2 boundary remains pending for structural runtime layouts, recursive
definitions, complete corpus integration and WebAssembly build/run.

## Compiler commits

| Milestone | Existing units and tracked source | Required local evidence |
|---|---|---|
| A.1, foundation | A1: `lib/foundation.bend`, `kernel_budget.bend`, `kernel_error.bend`, `kernel_level.bend`, `kernel_literal.bend` | Pinned host checks/builds for the landed modules, source-policy checks, and focused foundation, budget, error, level, and literal checks. |
| A.2, representation | A2: `kernel_term.bend`, `kernel_value.bend`, `kernel_shape.bend`, `kernel_global.bend`, `kernel_prim.bend`, plus the A.2 subsets of `kernel_quantity.bend` (tags), `kernel_positivity.bend` (records and constructor lookup), and `kernel_rules.bend` (arrow, unit, and boolean builders) | Build the landed dependency closure and exercise representation, lookup, and primitive contracts. |
| A.3, evaluation | A3: `kernel_eval.bend`, `kernel_conv.bend`, the runtime and conversion subset of `kernel_rules.bend`, quantity equality, and signed host index arithmetic | Evaluation and conversion cases covering normal forms, refusal and budget exhaustion, plus comparisons with the pinned oracle wherever an adapter is available. |
| A.4, checking | A4 through A6: the remaining rules, checking, positivity checking, the quantity algebra, printing, and specification counts | Focused acceptance/refusal, typing, shape, positivity, quantity, and printing checks, the specification counts, and earlier milestone regressions. |
| A.5a, test frontend parsing | A7 corpus and gate plumbing; A8 lexer, parser, and syntax printing in `test/pinfront/` | Every pinned corpus file and supplemental lexer/parser cases compared with freshly compiled pinned Kanon modules on Bun, Node worker, and native; malformed-output and isolated mutation checks. |
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

## Integration gate preservation

Retain the full pinned corpus, output and exit comparisons, divergence
policy, source policies, build-time check, mutants, fix round, and
final review required by M0-PLAN. Early source commits do not shrink the
corpus, waive an unrun check, introduce a semantic delta, or satisfy the
stage gate. R2 remains required under the entry decision's revised timing.
The production kernel follows the pinned-port plan; scratch
findings and test cases inform its implementation.
