# A.5b.3.2e.9: direct recursive-field layouts

This increment prepares the field metadata needed by the structural recursor.
It follows UAT.0 readiness at `27d4ab44c586fb3e87d48617079ea2a017998093`.
Public `elim` branch typing, delayed induction hypotheses and recursor erasure
remain pending. UAT.0 and Stage A acceptance stay open.

## Layout contract

`lib/kernel_recursor.bend` takes a checker context, a family name and a
constructor name. It uses the existing completed-family lookup, resolves the
constructor, and opens its parameters in a fresh context. Ambient locals do
not enter the declaration telescope.

Each field retains its zero-based constructor position, declared quantity,
name, normalized domain and fresh neutral value. A direct application of the
same family also retains the child's indices. Fields and the separate list
of induction-hypothesis candidates follow declaration order. Parameters do
not count as fields. Erased fields retain quantity zero in both lists.

The planner opens dependent fields in order and normalizes aliases. A
non-direct occurrence of the family in a normalized field type returns an
explicit refusal. This includes function-valued recursive fields. A field
type that mentions another member of the mutual group of the family also
returns an explicit refusal. The planner finds the group from the declared
families. It adds each family that has a constructor field that mentions the
owner or a family already in the group, until a round adds no family. A later
family that uses the owner is in this set, but no owner field can mention it.
A field whose type is a different family outside the group receives no direct
self hypothesis.

The result is metadata, not a recursor certificate. It does not establish
uniform parameter compatibility, mutual-recursion admissibility, branch
typing or termination of a generated recursor. The normalized domain remains
available for subsequent checks. The module adds no public source syntax or
evaluation rule.

## Validation

`make test-recursor-layout-mutations` passes nineteen fixed cases on Bun,
Node worker and native. The cases cover nullary constructors, multiple
recursive children, zero/one/many quantities, parameters, dependent fields,
indexed children, dependent indices, foreign families, aliases, later fields,
mutual groups (a sibling field, a constructor with self and sibling fields,
and a cycle of three families), a later family that uses the owner, and
lookup/refusal paths. Five separately typechecked and compiled mutants, each
run on Bun only, are killed by behavioral mismatches: field quantity, field
value, ambient-context leakage, non-direct-recursion refusal and a mutual
group reduced to the owner.

The test harness uses freshly checked reference-surface declarations. One
case supplies a synthetic field telescope against a checked transparent
alias to test normalization independently of declaration admission. Another
case preserves the pinned positivity checker's source-level refusal of an
alias application containing the declared family. These tests do not claim
that public structural recursors are accepted.

`make house test-reference-fixtures` passes all 26 Python tests and the scoped
source policy: 55 files, 44 line budgets, 63 existing reviewed unsafe sites and
41 existing catchalls. The new module adds neither unsafe sites nor catchalls.
`dev/makefile-graph.py` compares the Makefile with the Makefile at the
baseline commit. The comparison preserves the default `build` target and every
existing prerequisite of `check`, `test` and `gates`. The new normal check is
added to the prerequisite line of `check`, and its mutation check is added to
the prerequisite lines of `test` and `gates`.

The [result record](stage-a-recursor-layout.json) binds the inputs with SHA-256
and retains all host and mutation observations. The
[integration record](stage-a-recursor-layout-integration.json) records the
Makefile comparison. The adjacent `stage-a-recursor-layout-checks.stdout.log`
and `stage-a-recursor-layout-policy.{stdout,stderr}.log` retain command output.
The tests ran in an isolated checkout; publication verifies recorded inputs
against the destination index. The full M0 acceptance suite was not run for
this metadata increment and remains required at its integration gate.

Reproduce the scoped checks and the records with the commands below. The
harness writes its result record to `_build/recursor-layout/result.json`, and
the copy step publishes it. The checks log and the policy logs are `make`
transcripts.

```sh
make test-recursor-layout-mutations > dev/validation/stage-a-recursor-layout-checks.stdout.log
cp _build/recursor-layout/result.json dev/validation/stage-a-recursor-layout.json
make house test-reference-fixtures > dev/validation/stage-a-recursor-layout-policy.stdout.log 2> dev/validation/stage-a-recursor-layout-policy.stderr.log
python3 -P dev/makefile-graph.py --base 27d4ab44c586fb3e87d48617079ea2a017998093 --output dev/validation/stage-a-recursor-layout-integration.json
```
