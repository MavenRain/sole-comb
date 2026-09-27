# Stage A compiler milestones

Adopted on 2026-09-27. This replaces the whole-Stage-A commit boundary in
M0-PLAN section 11 and the matching user-commit schedule in section 12.
The S0-5 entry gate, pinned Kanon semantics, original
Stage A implementation units, and complete integration gate remain in force.
Stages B through F retain their existing stage boundaries.

## Entry

Start only after S0-5 step 4 writes `dev/r2-risk.json` with qualified
measurements, an eligible selected endpoint, any required GREEN
confirmation, and GREEN or AMBER evidence. On FAIL, Stage A does not start
until the user rules in writing.
Commit boundaries do not waive this entry condition.

## Compiler commits

| Milestone | Existing units and tracked source | Required local evidence |
|---|---|---|
| A.1, foundation | A1: `lib/foundation.bend`, `kernel_budget.bend`, `kernel_error.bend`, `kernel_level.bend`, `kernel_literal.bend` | Pinned host checks/builds for the landed modules, source-policy checks, and focused foundation, budget, error, level, and literal checks. |
| A.2, representation | A2: `kernel_term.bend`, `kernel_value.bend`, `kernel_shape.bend`, `kernel_global.bend`, `kernel_prim.bend` | Build the landed dependency closure and exercise representation, lookup, and primitive contracts. |
| A.3, evaluation | A3: `kernel_eval.bend`, `kernel_conv.bend` | Evaluation and conversion cases covering normal forms, refusal and budget exhaustion, plus comparisons with the pinned oracle wherever an adapter is available. |
| A.4, checking | A4 through A6: rules, checking, positivity, quantity, printing, and specification counts | Focused acceptance/refusal, typing, shape, positivity, quantity, and printing checks, the specification counts, and earlier milestone regressions. |
| A.5, oracle integration | A7 through A10: development gates and the pinned Kanon test frontend | Reproducible builds and a runnable differential harness over the complete pinned corpus and modes. |
| A.close, Stage A acceptance | A11 through A13: full verification, fixes, and review close | Every original Stage A gate and mutant check, complete KANON-DIFF under the existing divergence policy, HOUSE, R2-RISK-A, and BUILD-TIME. |

Required build and test plumbing may accompany the first milestone that
needs it, even when its original unit is A7. Record that dependency move;
it does not remove A7's remaining deliverables or change any gate.

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
policy, source policies, build-time check, R2-RISK-A, mutants, fix round, and
final review required by M0-PLAN. Early source commits do not shrink the
corpus, waive an unrun check, introduce a semantic delta, or satisfy the
stage gate. The production kernel follows the pinned-port plan; scratch
findings and test cases inform its implementation.
