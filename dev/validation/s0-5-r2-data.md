# S0-5 scratch non-indexed data checking

Recorded on 2026-09-27 UTC. This record covers the next scratch checker pass
after the [positional arm expansion pass](s0-5-r2-arms.md). The pass adds
non-indexed data types with parameters to copies of the scratch conversion
engine and typechecker. It follows M0-PLAN sections 5.1 to 5.3 and 6.4:
constructor spines check only in checking mode, `elim` takes an explicit
`as x return M` motive, and K1 iota reduces an elimination of a saturated
constructor.

This is frontend correctness evidence. It does not qualify a probe workload,
select an endpoint, establish an R2 verdict, or start the Stage A kernel port.

## Scratch artifacts

Sources remain outside this repository under
`/private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/`:

- `data-conversion.bend`, `data-values.bend`, `data.bend`: copies of
  `conversion.bend`, `typing-values.bend` and `typing.bend` with the data
  rules. The committed conversion and typing records pin the originals, so
  the pass does not change them.
- `data-fixtures.py`: exact signature trees, refusal results, and nine
  targeted mutations.
- `validate-data.py`: pinned builds, three runtime checks, source policy,
  source and generated-artifact hashes, and mutation checks.
- `data-policy.json`: the data contract; no unsafe declarations or catchalls
  in the data modules.
- `audit-data-record.py`: independent hash and evidence-matrix audit.
- `data-validation-002/`: final source snapshots, generated checks, bundles,
  raw stdout/stderr, fixtures, and `validation.json`.

The committed [JSON record](s0-5-r2-data.json) is a byte copy of that
`validation.json`, SHA-256
`70cc67b0e9827cb52085d46451f0419da98ffd163764b4a9d006773d957c6827`.
It binds the three data modules, the inherited frontend modules, both
fixture generators, the validation harness, the source policies, the
repository build and pin-check helpers, and the toolchain file. It also
binds every generated test, module snapshot, bundle and raw stream by
SHA-256 hash. The independent auditor is separate from those validation
inputs; its SHA-256 at audit time was
`9383d499bd1171ca95f2e4a1c1ac8fe9074ca32c022430a2bb9beac7ce488822`.

## Supported contract

`data_check(lex, parse, resolve, fuel, specs, signature, source)` takes
caller metadata for each data type: the former name, the parameter count,
and the constructor names in declaration order. The language has no data
syntax yet, so the caller supplies this metadata, as for the arm expander.
The signature source gives the former and constructor types as ordinary
definitions, former first; the checker ignores their values.

The checker refuses a signature definition that the metadata does not name.
A former must end in a universe after its parameters; `Prop` data is
refused. A constructor field can mention the data type only as the exact
recursive field `D p1 .. pk`. Other mentions fail the positivity check. A
constructor must end in `D p1 .. pk`, and a field sort above the data
universe is refused.

A constructor spine checks against `D p1 .. pk` in checking mode: the
parameters come from the expected type, and each field checks against the
constructor telescope. A constructor in inference position gives
`CannotInfer`. Constructor values hold their fields only.

`elim s as x return M { c1: t1; ...; cn: tn }` infers the type `D ps` of
`s` and checks `M` as a sort under `x : D ps`. Arm `k` must carry the label
of constructor `k`. It checks against the fields of that constructor, one
induction hypothesis `M[r/x]` for each recursive field `r`, and the result
`M[c fields/x]`. The elimination has type `M[s/x]`. A missing motive gives
`Unsupported "motive inference"`.

Evaluation applies K1 iota when the scrutinee is a constructor with all
of its fields. It applies arm `k` to the fields, then to one strict
induction hypothesis for each recursive field. Any other scrutinee gives a
stuck elimination. Read-back gives an `Elim` normal form, and conversion
compares stuck eliminations structurally.

## Validation

The final run passed 40 cases in five batches on Bun, the pinned Node worker,
and the informational native build. Coverage includes constructor checking and
the inference refusal, elimination with induction hypotheses, iota by
conversion for `Bool`, `Nat`, `List` and `Option`, stuck eliminations on a
local, dependent motives, an empty type with zero arms, elimination refusals,
signature refusals, and zero typing fuel. All 14 baseline and mutant Bend
checks reported success, so the mutation results are behavioral failures rather
than compile failures.

Bun detected all nine mutations: iota without induction hypotheses, a shifted
arm selection, a wrong motive instance for the result, no motive sort check, no
arm body check, no induction hypothesis binder in the arm type, no parameter
read from the expected type, constructor type inference, and no arm count
check. The independent audit passed all 256 references and the complete case,
endpoint, mutation, and check matrices.

Run `data-validation-001` passed all 40 cases, but Bun did not detect the
shifted arm selection. Its target case had two eliminations whose shifts
cancelled. Run `data-validation-002` targets that mutation at
`bool-iota-first-arm-refused`, and Bun detects it there.

An earlier design applied iota to parameters and fields. Source constructors
carry their fields only, so parametric iota stayed stuck. The pass now keeps
parameters out of constructor values and out of the arm result instance.

`make gates` passed in a non-git copy of repository HEAD `0312350` at
`/private/tmp/claude/kan-elim-lang-m0/sole-comb-data-close`: 143 regression
tests, 35 toolchain checks, and the host check. The repository keeps the
[stdout](s0-5-r2-data-gates.stdout) and [stderr](s0-5-r2-data-gates.stderr) of
that run. Repository executable code did not change. The data modules contain
no unsafe declarations or catchall arms; the lexer and parser keep only their
six previously recorded numeric-dispatch catchalls.

Reproduce into a fresh directory:

```sh
python3 -P /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/validate-data.py \
  --output /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/data-validation-003 \
  --mutations
python3 -P /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/audit-data-record.py \
  /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/data-validation-003/validation.json
```

## Remaining boundary and next step

The checker takes data metadata from the caller and has no data syntax.
Indexed families, motive inference, lazy K1d reduction, records,
projections, the connection to the arm expander, and recursive sugar
processing remain pending. Eta, proof irrelevance, portable file input,
the two source twins, their qualification, and valid measurements also
remain pending. The work has selected no endpoint and no R2 verdict.
