# Stage A compiler milestones

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
checking, A.5a test frontend parsing, and A.5b.1 expression/declaration
elaboration are implemented. Their scoped checks and representation choices
are recorded in
[A.1 validation](validation/stage-a-foundation.md),
[A.2 validation](validation/stage-a-representation.md),
[A.3 validation](validation/stage-a-evaluation.md),
[A.4 validation](validation/stage-a-checking.md),
[A.5a validation](validation/stage-a-pinfront.md), and
[A.5b.1 validation](validation/stage-a-elaboration.md).
The next implementation milestone is A.5b.2 recursive program and full oracle integration.

## Compiler commits

| Milestone | Existing units and tracked source | Required local evidence |
|---|---|---|
| A.1, foundation | A1: `lib/foundation.bend`, `kernel_budget.bend`, `kernel_error.bend`, `kernel_level.bend`, `kernel_literal.bend` | Pinned host checks/builds for the landed modules, source-policy checks, and focused foundation, budget, error, level, and literal checks. |
| A.2, representation | A2: `kernel_term.bend`, `kernel_value.bend`, `kernel_shape.bend`, `kernel_global.bend`, `kernel_prim.bend`, plus the A.2 subsets of `kernel_quantity.bend` (tags), `kernel_positivity.bend` (records and constructor lookup), and `kernel_rules.bend` (arrow, unit, and boolean builders) | Build the landed dependency closure and exercise representation, lookup, and primitive contracts. |
| A.3, evaluation | A3: `kernel_eval.bend`, `kernel_conv.bend`, the runtime and conversion subset of `kernel_rules.bend`, quantity equality, and signed host index arithmetic | Evaluation and conversion cases covering normal forms, refusal and budget exhaustion, plus comparisons with the pinned oracle wherever an adapter is available. |
| A.4, checking | A4 through A6: the remaining rules, checking, positivity checking, the quantity algebra, printing, and specification counts | Focused acceptance/refusal, typing, shape, positivity, quantity, and printing checks, the specification counts, and earlier milestone regressions. |
| A.5a, test frontend parsing | A7 corpus and gate plumbing; A8 lexer, parser, and syntax printing in `test/pinfront/` | Every pinned corpus file and supplemental lexer/parser cases compared with freshly compiled pinned Kanon modules on Bun, Node worker, and native; malformed-output and isolated mutation checks. |
| A.5b.1, elaboration entry points | A9 through A10 expression and declaration elaboration in `test/pinfront/`, ordinary declaration checking, and mutual family admission | Exact pinned declaration observations for all 146 corpus files and focused probes on Bun, Node worker, and native; isolated semantic mutations. Recursive groups exercise the declaration API refusal. |
| A.5b.2, full oracle integration | Remaining A7 plumbing and A9 through A10 recursive program and frontend integration | Reproducible builds and a runnable kernel differential harness over the complete pinned corpus and modes. |
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
CLI output/exit modes, and the full KANON-DIFF remain A.5b.2 and A.close work.

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
