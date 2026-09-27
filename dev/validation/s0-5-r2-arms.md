# S0-5 scratch positional arm expansion

Recorded on 2026-09-27 UTC. This record covers the next scratch checker pass
after the [function-fragment typing pass](s0-5-r2-typing.md). The pass
implements the first part of the remaining sugar work. It follows M0-PLAN
sections 6.1 and 6.2: optional constructor labels confirm positions, and a
final `else: t` expands to positional arms before elaboration. Labels never
reorder arms.

This is frontend correctness evidence. It does not qualify a probe workload,
select an endpoint, establish an R2 verdict, or start the Stage A kernel port.

## Scratch artifacts

Sources remain outside this repository under
`/private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/`:

- `arms.bend`: exhaustive sums and a single fuel-bounded state machine.
- `arms-fixtures.py`: exact syntax trees, refusal results, parser-to-expander
  fixtures, fuel boundaries, and twelve targeted mutations.
- `validate-arms.py`: pinned builds, three runtime checks, source policy,
  source and generated-artifact hashes, and mutation checks.
- `arms-policy.json`: no unsafe declarations or catchalls in the expander.
- `audit-arms-record.py`: independent hash and evidence-matrix audit.
- `arms-validation-004/`: final source snapshots, generated checks, bundles,
  raw stdout/stderr, fixtures, and `validation.json`.

The committed [JSON record](s0-5-r2-arms.json) is a byte copy of that
`validation.json`, SHA-256
`ff12724f72525cb7ab9cbfa9aba467cd2089bf0eaf12ae01ad0e956bb06a0ebc`.
It binds the expander, both inherited frontend modules, fixture generator,
validation harness, source policies, repository build and pin-check helpers,
and toolchain file. It also binds every generated test, module snapshot,
bundle and raw stream by SHA-256 hash. The independent auditor is separate
from those validation inputs; its SHA-256 at audit time was
`5259009cc71846def780ec5b33b90acd14bc6356d0507a4a2e2c129988397c5b`.

## Supported contract

`expand(fuel, constructors, arms)` consumes one parser arm list and a supplied
list of constructor names in declaration order. Constructor names must be
nonempty, unique, and different from the reserved default label `else`.
The pass checks those metadata conditions before processing arms. It does
not establish that the names describe the scrutinee's type.

An unlabeled arm consumes the next constructor position. A labeled arm must
name exactly that position, with case-sensitive comparison. A final `else`
copies its ordinary term into every remaining position. Output labels are
empty after validation. The expander keeps the arm order and the complete
branch syntax, including lambda binders, references to the surrounding
scrutinee, motives, projections, and nested eliminations. It adds no implicit
binders.

Missing or excess arms, mismatched labels, invalid or duplicate constructor
names, a nonfinal default, and a redundant default are distinct errors.
For this scratch subset, `else` must cover at least one position. An empty
shape accepts an empty arm list and rejects a default or explicit arm.

One shared Nat budget charges each state transition, including constructor
validation, duplicate scans, expansion, default filling, and output reversal.
Exhaustion returns `Exhausted`, never a partial successful list. Exact fuel
checks include 3 transitions for the empty list, 7 for one explicit arm,
and 13 for a default covering two constructors. String comparisons use the
inherited structurally recursive comparator; fuel does not count characters.

## Validation

The final run passed 57 cases in eight batches on Bun, the pinned Node worker,
and the informational native build. Coverage includes positional and mixed
labels, order preservation, full and partial default coverage, all parser term
forms as untouched bodies, all eight error constructors, fuel boundaries,
and five source fixtures passed through the actual lexer and parser before
expansion. All 20 baseline and mutant Bend checks reported success, so the
mutation results are behavioral failures rather than compile failures.

Bun detected all twelve mutations: accepting a wrong label, retaining a
checked label, reversing output order, accepting duplicate or invalid
constructor metadata, accepting missing or excess arms, accepting nonfinal
or redundant defaults, dropping a default arm, changing a default body,
and returning success on exhaustion. The independent audit passed all
294 references and the complete case, endpoint, mutation, and check matrices.

`make gates` passed in a non-git copy of repository HEAD `ab01b81` at
`/private/tmp/sole-comb-arms-close`: 143 regression tests, 35 toolchain checks,
and the host check. The repository keeps the
[stdout](s0-5-r2-arms-gates.stdout) and [stderr](s0-5-r2-arms-gates.stderr) of
that run. Repository executable code did not change. The new scratch sources
contain no unsafe declarations or catchall arms; the lexer/parser retain only
their six previously recorded numeric-dispatch catchalls.

Reproduce into a fresh directory:

```sh
python3 -P /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/validate-arms.py \
  --output /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/arms-validation-005 \
  --mutations
python3 -P /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/audit-arms-record.py \
  /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/arms-validation-005/validation.json
```

## Remaining boundary and next step

This helper expands one arm list, not an entire program. Nested eliminations
in a branch remain unchanged until a caller supplies their own constructor
metadata. The typechecker still refuses elimination and projection. The
parser-to-expander fixtures supply metadata explicitly; they are not data
typing tests or proof that the whole checker now accepts `elim`.

Next, implement the scratch data and projection rules needed by the twins,
connect this helper where constructor metadata is available, and add recursive
sugar processing and record expansion. Typed conversion, portable file input,
the two source twins, their qualification, and valid measurements remain
pending. The work has selected no endpoint and no R2 verdict.
