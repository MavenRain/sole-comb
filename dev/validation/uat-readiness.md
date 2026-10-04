# UAT.0: pinned proof baseline and public-source readiness

This increment completes the baseline-preparation deliverables of the UAT.0 row
for the selected UAT pilot dependencies. It adds source probes and a reproducible
Lean inspection. The UAT.0 row stays open until the public structural recursors
land. They remain the next Stage A compiler prerequisite. UAT.1 through UAT.5
remain implementation work under the existing M1 semantic-delta schedule.

## Baseline and assumptions

The UAT revision is `f9d2bc270631eaefb9985c38e0c804b825a7ee2d`, with
`leanprover/lean4:v4.31.0`. `dev/uat-baseline/uat-baseline.json` pins all ten
dependencies from that revision's Lake manifest, including transitive tooling
dependencies. It also records the hashes of tracked Lean and configuration
sources in each checkout. The capture refuses a different dependency revision
or dirty tracked sources. It neither fetches nor updates dependencies.

The selected modules are `UnifiedAggregation.Characterization`,
`UnifiedAggregation.FunctorExt` and `UnifiedAggregation.Z2Group`.
`test/uat-baseline.lean` inspects 21 declarations using `#check` with explicit
universes and full names, followed by `#print axioms`. The complete elaborated
statements and transitive axiom results are in `uat-baseline.stdout`.

| Selected declarations | Port requirement |
|---|---|
| `Category`, `Functor`, `LeftKanExtension` | UAT.2 dependent records, UAT.3 object/morphism universes, UAT.4 categorical universal properties |
| `SymmetryGroup`, `GAction`, `Z2Group` | UAT.2 dependent operations and proof fields, UAT.5 concrete action |
| `eqRec_heq_dep`, `OrbitHom.ext` | UAT.1 dependent transport and heterogeneous equality, UAT.4 morphism laws |
| `Functor.ext`, `Functor.ext_pointwise` | UAT.4 dependent functor and function extensionality |
| `orbitProjection`, `Aggregation` | UAT.4 categorical library and UAT.5 aggregation witness |
| `discreteFunctor`, `discreteHom_eq` | UAT.2 indexed dependent data and UAT.5 discrete category |
| `Z2.one_mul`, `mul_one`, `mul_assoc`, `mul_inv`, `inv_mul` | UAT.5 checked group laws |
| `lan_obj_unique_discrete`, `lan_implies_orbit_constant` | UAT.4 universal-property reasoning and UAT.5 orbit correspondence |

The 13 empty axiom sets are `Category`, `Functor`, `SymmetryGroup`, `Z2Group`,
the five Z2 group laws, `Functor.ext`, `eqRec_heq_dep`, `discreteFunctor` and
`discreteHom_eq`. The eight nonempty sets are:

- `Functor.ext_pointwise`: `Quot.sound` (the output retains its universe argument).
- `LeftKanExtension`, `GAction`, `OrbitHom.ext`, `orbitProjection`,
  `Aggregation`, `lan_obj_unique_discrete`, `lan_implies_orbit_constant`:
  `propext`.

None of these 21 declarations depends on `sorryAx` or `Classical.choice`.
This is a selected dependency inventory, not a census of every UAT theorem.
In particular, Arrow, the rational/finite prelude and the full bridge modules
remain the full-port backlog. A port must account for these baseline assumptions
and prove the needed relationship between the universal-property
`LeftKanExtension` and native `Lan`. This increment adds no proof axiom and
marks no Lean theorem as ported.

## Public-source probes

`examples/uat-foundation.sole-comb` supplies six checked definitions: Z2 flip
and multiplication, a Box value and projection, a List head, and reflexivity
for Nat-indexed equality in `Type 0`. The latter establishes ordinary index
checking; it does not satisfy equality in `Prop` or prove any group law.
Both checked and erased output must agree across the three hosts.

`dev/uat-readiness.json` maps every probe to its requirement and milestone.
It also records the complete diagnostic of each refusal. The eleven refusal
files under `test/uat/` cover:

- UAT.1: Nat-indexed and type-parameterized equality in `Prop` currently fail
  the index/universe admission rule. Unequal endpoints remain a required refusal.
- UAT.2: dependent fields are outside the closed-record expansion; parameterized
  records lack public syntax; an extra recursive-field induction-hypothesis
  binder fails the existing branch-result check. A nonstructural self-reference
  is unbound and remains a required refusal.
- UAT.3: universe and instance declarations lack public syntax. These two probes
  describe source admission only, not universe-solving or resolution behavior.
- UAT.0: a phantom constructor parameter without an expected type or field
  evidence remains a required refusal. Field-based inference is already landed.
  `def rec` remains excluded: the parser reserves `rec` and points to `elim`.

The readiness gate requires source-refusal exit 1, empty stdout and a stderr
line equal to the mapped diagnostic. Build failures, timeouts, host errors and
unrelated or altered refusals cannot satisfy a probe. It preserves each stdout,
stderr and exit observation, compares hosts, checks the source census and binds
the result to source hashes.
When a prerequisite lands, revise its readiness disposition explicitly and add
the acceptance corpus required by that milestone. These probes are not the
complete future transport, positivity, singleton-elimination or recursor suite.

## Reproduction

`make test-uat-readiness` validates the frozen baseline, harness refusals and
public-source probes on Bun, Node worker and native. `make check`, `make test`
and `make gates` include this target and retain every existing prerequisite.
Routine gates do not need a Lean installation or a UAT checkout.

For a local baseline rebuild and byte comparison, use:

```sh
python3 -P dev/test-uat-readiness.py --baseline-checkout /path/to/unified-aggregation-theory
```

The checkout must contain the locked local dependencies. The optional check
uses `leancho` to rebuild only the three selected modules, reruns the Lean
inspection and compares the complete new record with the frozen baseline.
The record keeps the Lean release and commit and omits the platform triple.
It writes inspection artifacts under `_build/uat-readiness/baseline`.

## Validation

Validation ran on a scratch copy of the staged tree based on sole-comb
`86251ec995440f4f644a495f1c1d8523f7c952af`. The final frozen-input run of
`make test-uat-readiness test-cli house test-reference-fixtures` exited 0:
thirteen readiness-harness tests, 39 readiness observations across all three
hosts, 104 public CLI inputs and 325 command observations, the unchanged host
policy, and eleven reference-fixture tests passed.

The optional live Lean check also exited 0. Its complete regenerated baseline
record equals the frozen record, and all 39 public-source observations passed
again. The scoped Bend compiler check and all 34 toolchain pin checks passed.
The selected Lean build reported zero errors, sorries and warnings.

Saved evidence is `uat-readiness.json`, `uat-readiness-cli.json`,
`uat-readiness-checks.{stdout,stderr}.log`, and the
`uat-readiness-{live,build,pins}.stdout.log` files in this directory. The two
observation records carry 225 source hashes in total, with overlapping inputs.
The readiness result line names the repository-relative path
`_build/uat-readiness/result.json`. The live baseline line and the build and pin
logs still include the scratch paths. The full `make gates` target was not rerun
for this preparation increment; the production Bend sources are unchanged.
Full Stage A acceptance and operational-compiler gates remain pending.
