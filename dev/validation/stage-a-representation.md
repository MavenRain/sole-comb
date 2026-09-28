# Stage A.2 representation

Implemented on 2026-09-28, on top of foundation commit
`824073f340189e4329747bded51d58686022a479`.
This is the A.2 milestone in [the schedule](../stage-a-milestones.md).
It preserves Kanon `69f3be5198cda4334de4fd23b4ccc92cac595789` semantics
under M0-PLAN section 11, Stage A. The later pin deltas remain pending.

## Source scope

| Module under `lib/` | Physical lines | Plan limit | Implemented contract |
|---|---:|---:|---|
| `kernel_term.bend` | 130 | 180 | All 13 terms, addresses, legs, motives, eliminations, total address views, and the occurrence walk |
| `kernel_value.bend` | 143 | 220 | All seven value forms, captured environments, universe slots, stuck eliminations, newest-first spines, and total views |
| `kernel_shape.bend` | 46 | 90 | All five shapes, ordered payloads, point-domain and family views |
| `kernel_global.bend` | 91 | 180 | Persistent entry and family maps, typed lookups, entry views, and the initial environment |
| `kernel_prim.bend` | 122 | 200 | The five Nat primitives, types, exact saturation, literal reduction, and collection-valued comparisons |
| `kernel_quantity.bend` | 7 | 260 | The three quantity tags required by the representation |
| `kernel_positivity.bend` | 31 | 180 | Telescope, constructor and family records, and constructor lookup |
| `kernel_rules.bend` | 24 | 2100 | Arrow, unit and boolean builders required by the primitive catalog |

Plan limits are the section 4 budgets that `dev/house-bend.py` enforces.
The `kernel_value.bend` limit of 220 includes the K1d increment of 40.
Section 3.1 and the section 11 A2 total of 850 lines use 200.

The final three rows move only A.2 dependencies forward from A.4.
The quantity usage algebra, positivity checking, and the remaining shape
and checking rules are still pending. Family records do not certify
positivity. The complete primitive catalog and recursive-definition flags
remain present, as required by the Stage A port before any pin delta.

The reference is `kanon/lib/{term,value,shape,global,prim}.ml`, with the
supporting declarations from `quantity.ml`, `positivity.ml:24-60`, and
`rules.ml:257-284`. [The record](stage-a-representation.json) binds all eight
reference files by SHA-256. The generic data-carrier pattern follows
attest `lib/foundation.bend` at source SHA-256
`30dfb2d9d09081405072c49c99c50c0e5b5997dfe9447061fad2dc83cc1dcee3`.

These pinned host integers use the foundation `Int63.t` carrier: term
indices, levels of locals, leg addresses (`ALeg`, `VALeg`), the collection
size (`SColl`), and constructor arities (`c_full_arity`).
They preserve negative host inputs until the later checking rules refuse
them. `Level.t` continues to mean universe levels. Generic data carriers
break the host type-definition cycles without removing any pinned field.
Named `Former` and `Intro` products carry the pinned view tuples.
Other pinned pairs, including the results of `as_sec`, `as_neutral`, and
`as_pt`, use `F.Pair2`. A telescope entry nests the pinned triple as
`F.Pair2<Q.t, F.Pair2<String, T.t>>` in the same order. The anonymous
entry payload records are named `Definition`, `Postulate`, and `Primitive`.
The global definition field is named `body`, because `def` is a Bend keyword.
Primitive function-valued dispatch rows become exhaustive matches with the
same answers. No abstract type boundary is claimed: exported constructors
retain the foundation milestone's invariant-by-convention limitation.

`kernel_term.scan` walks an explicit stack in the pinned order. Each step
replaces one term by its strict subterms, decreasing the total term-node
count in the pending forest by one. This is the one new host `@unsafe`
site in [the registry](../bend-policy.json). Bend cannot derive that measure
through `scan_term`. Type, affine-use, and exhaustive-match checks remain
enabled. No module outside `kernel_shape.bend` and `kernel_rules.bend` matches
on a shape constructor name.

## Validation and bounded review

Command: `make gates`, run after the review fixes in an isolated copy that
keeps `.git`:
`/private/tmp/claude/kan-elim-lang-m0/review-0928-stage-a-representation/gates-copy-postfix`.
The first run used the validation copy
`/Users/oobi/Documents/gpt5/sole-comb-stage-a2`.
The published source hashes are checked against the successful run.
The complete output is retained as
[stdout](stage-a-representation-gates.stdout) and
[stderr](stage-a-representation-gates.stderr).

- 150 Python regression tests passed, including seven policy tests.
- HOUSE passed: 14 host files, 13 physical line budgets, three registered
  unsafe sites, and zero catch-all sites.
- All 219 A.1 cases passed on Bun, the Node worker, and native (INFO).
- All 465 A.2 cases passed on each of those three hosts.
- All 35 pin checks passed; the host-entry build check passed.

`make test-representation` runs the A.2 checks by themselves. `make check`,
`make test`, and `make gates` all include them. Native remains informational
for endpoint selection: it is not a counted endpoint, but a native compile
failure, crash, or wrong answer still fails the run. The generated harness
groups 32 cases per function to stay below the pinned native backend's
function-arity limit.

The bounded review compared all closed sums and payload fields with the
pinned source, followed every total view, checked the occurrence-walk
child order and termination measure, and checked primitive saturation,
negative-literal refusal, Nat subtraction and the boolean encodings.
It also checked that each map write preserves the other table.
The suite exercises every value/view combination, every address view,
each occurrence-bearing child position, closures and stuck elimination
payloads, spine order, global replacement, family lookup and replacement,
large arithmetic, both comparison outcomes, and malformed applications.
Python integers provide the arithmetic expectations.

The validation record binds 24 source and gate inputs plus the generated
harness hash. It is a snapshot of this run; the gates do not automatically
compare it with a later checked-in record. The original A.1 record remains
historical evidence for that commit.

A.3 evaluation and conversion is next. The bootstrap CLI still does not
check source programs. Full KANON-DIFF, the 146-file corpus, the complete
Stage A mutants and review, BUILD-TIME, and operational-compiler R2
qualification and measurement remain pending. This local milestone is
neither A.close nor an endpoint selection.

## Review fixes, 2026-09-28

The bounded review of the staged slice found eight items. All are fixed.

- The A.2 and A.1 harnesses now refuse a run unless the case list has
  exactly the pinned count (465 and 219) of uniquely named cases. Before,
  an empty or shortened list still printed PASS.
- Four new A.2 cases put the match after the first list item: a second
  section leg, a second elimination branch, and a second name for a term
  and for a family. A mutant of `leg_terms`, `branch_terms`, or `member`
  that ignores the rest of its list fails only its new cases, so the
  previous 461 cases did not detect it.
- The milestone table now lists the A.2 subsets of the quantity,
  positivity, and rules modules.
- This record now names each carrier and pair change, the source of each
  plan limit, the exact shape-dispatch claim, and the native failure rule.
