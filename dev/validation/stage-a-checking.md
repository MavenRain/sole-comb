# Stage A.4 checking

Implemented on 2026-09-28, on top of A.3 commit
`331e6623bd5392a8b96b0c1048ae0f7e91cb7253`. This is the checking milestone in
the [Stage A schedule](../stage-a-milestones.md). A.5 oracle integration is next.
The complete Stage A corpus gate and R2 qualification remain pending.

## Implementation

`kernel_check.bend` provides bidirectional checking, local and global contexts,
let aliases, universe inference, conversion, sequential declarations, family
declarations, and constructor admission. The remaining rule operations in
`kernel_rules.bend` cover Pi, collection, and mu formation, introduction and
elimination, motives, branch coverage, telescope application, constructor
indices, and the pinned large-elimination restrictions.

The quantity algebra keeps the pin's three tags and separates returning paths
from syntactic reads. Intervals use sorted association lists in place of the
OCaml integer map. Positivity checking stores the same constructor metadata.
The printer preserves binder scope and OCaml's byte escaping, including UTF-8
strings. Specification rows derive from the term, shape and rule inventories:
two formers, four schema constructors, and three admitted shapes.

The semantic reference is Kanon
`69f3be5198cda4334de4fd23b4ccc92cac595789`. No Stage B quantity, universe,
recursor, or shape changes are included. The pinned refusal behavior remains.

Checking extends the existing typed request interpreter. Conversion's
proposition probe now requests universe inference from the shared checker
dispatcher; standalone conversion still uses its supplied inference callback.
Failures pass back through continuations, so proposition probes can recover
from a failed inference without discarding the spent budget.

The host adaptation is a pure finite inference budget instead of OCaml's
mutable polling closure. Each inference request consumes one tick. The driver
threads the remaining budget through nested checking and conversion requests;
`infer_with_budget` and `check_with_budget` expose it to callers. As at the pin,
this is an inference poll boundary, not a bound on every evaluation step.
Unchecked raw semantic inputs retain the evaluator's possible divergence.
The Bend test harness gives each checking context a budget of 512 inference
requests, but the OCaml oracle uses `Budget.unlimited`. A case that uses all
of the Bend budget fails the gate with a budget error, so it cannot pass
vacuously.

Two new host recursion exceptions are registered in `../bend-policy.json`:
the finite printer worklist and the checker request interpreter. The audit
accepts nine documented host exceptions across the current library and no
catch-all matches. Type, affine-use, and exhaustiveness checks remain enabled.

| Module | Physical lines | Stage A limit |
|---|---:|---:|
| `kernel_check.bend` | 410 | 800 |
| `kernel_rules.bend` | 1341 | 2100 |
| `kernel_quantity.bend` | 198 | 260 |
| `kernel_positivity.bend` | 97 | 180 |
| `kernel_pp.bend` | 148 | 150 |
| `kernel_spec_count.bend` | 36 | 90 |

## Validation

`make gates` passed on an isolated copy of the staged tree that keeps `.git`.
The run took 158 seconds. The one-minute load average was 22.28 at the start
and 17.26 at the end. No upstream checkout or compiler installation was modified.
The saved [stdout](stage-a-checking-gates.stdout.log) and
[stderr](stage-a-checking-gates.stderr.log) retain the complete run.

- 150 Python regression tests passed for build, pins, benchmark infrastructure,
  R2 tooling and source policies. All 35 pin checks passed.
- A.1's 219 cases, A.2's 465 cases, and A.3's 251 cases passed on Bun,
  Node worker, and native execution.
- A.4's 291 uniquely named cases passed on the same three hosts, including
  125 fixed expectations checked against the independently compiled oracle.
- Three isolated mutations were rejected on Bun after their unmodified
  controls passed: skipping linear closure checks, refunding nested inference
  budgets, and disabling the negative-position positivity refusal.
- The bootstrap host entry passed its pinned compiler check, and the source
  audit passed all 18 landed module budgets.

The A.4 cases cover quantity tables and intervals, multi-level intervals,
every printer constructor, printer lists with more than one element, Unicode
escaping, positivity, type checking and refusal, linear products and
alternatives, let aliases, Pi and collection eliminations, empty collection
elimination, declaration order, family admission, parameters and indices,
mu motives and coverage, large-elimination restrictions, and proof
irrelevance through the actual checker. Budget cases include a nested let
that detects the refund mutation. They do not claim the complete surface
corpus or exhaustive combinations of these rules.

`dev/test-checking.py` freshly compiles the pinned OCaml dependency closure,
verifying every copied `.ml` and available `.mli` against its Git object.
The adapter uses the public Level interface. It requires the fixed case count,
the fixed count of fixed expectations, unique names, ordered oracle rows, all
fixed expectations, and exact PASS
output from each Bend endpoint. Native execution is required to pass; its
INFO label only preserves the endpoint-selection convention.

The [checking record](stage-a-checking.json) hashes all library files, harness
inputs, build and pin machinery, source policy, and Makefile, plus the
generated harness and the [oracle observations](stage-a-checking-oracle.tsv).
The [mutation record](stage-a-checking-mutations.json) binds each mutated
source and selected case to that checking record. The mutation runner requires
fresh baseline hashes and cannot accept a compile failure as a killed mutant.
It changes only copied sources under `_build/checking-mutations/`.

The bounded local review covered request dispatch and fuel propagation,
conversion error recovery, linear scope closure, family refusal paths,
inventory-derived counts, test provenance, and the Makefile/source-policy
diff. It found no unresolved issue in this scope. Existing gates were retained;
checking and mutation gates were added. Earlier milestone records remain
historical evidence rather than being rewritten with new hashes.

## Remaining boundary

The CLI remains the bootstrap entry. A.5 must connect the pinned surface test
frontend and complete corpus/mode harness. The 146-file corpus, full Stage A
integration and mutation gates, R2 operational qualification, and endpoint
selection are not completed by these focused checks.

## Review fixes, 2026-09-28

- Fifteen usage cases now use two or more interval levels. They exercise
  the merge and remove arms of the sorted association list.
- Five printer cases now use lists with two elements: binders, legs, `In`
  arguments, motive indices, branches without a motive, and `SNu` elements.
- `dev/test-checking.py` now requires exactly 125 fixed expectations. Before
  this fix, the record wrote the count but the harness did not check it.
- This record now states the 512 request budget of the Bend harness and the
  unlimited budget of the oracle.
- The gate rerun used an isolated copy of the staged tree that keeps `.git`.

No commit was created.
