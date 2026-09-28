# S0-5 workload parser extension

The step 2 grammar prerequisite passed on 2026-09-27. The extended scratch
Bend parser preserves the complete syntax trees of the frozen `core.sole`
and `smoke.sole`, plus `bad-bound.sole`, `bad-constructor.sole`, and
`false-law.sole`. This is parser correctness evidence. Source checker
qualification, an R2 verdict, and endpoint selection remain pending.

## Source and grammar

The retained source directory is
`/private/tmp/claude/kan-elim-lang-m0/r2-risk/workload-parser-v2/`.
It extends the prior scratch parser and retains its lexer. The earlier
component sources and records remain unchanged. The new parser has explicit
nodes for data declarations, literals, empty sum/product/tuple forms, and
indexed motives. It retains names and signatures for later resolution.

| Syntax | Retained information |
|---|---|
| `mu D (A : Type) : (i : I) -> Type { C : ... }` | Data name, ordered typed parameters, complete result telescope, and ordered constructor names and signatures |
| `elim t as value in D i return R { ... }` | Scrutinee, value binder, family expression, result expression, and ordered arms |
| Scratch literal | Original decimal digits, including leading zeros |
| `sum ()`, `prod ()`, `tuple ()` | Three distinct zero-arity nodes |
| Empty elimination | Explicit motive and an empty arm list |

Nonempty sum/product/tuple syntax remains outside this slice. A literal
outside the U32 range still parses: range checking belongs to typing.
Likewise, the parser preserves an indexed family expression without
checking its kind or scope. It does not establish constructor validity,
positivity, bounds, recursion behavior, or equality proofs.

The implementation retains one unit of fuel per parser machine transition.
New parameter and constructor loops use machine states. Exhaustion returns
a typed error at the current token, with no partial successful program.
The policy audit scans `parser.bend`. It finds no unsafe source and exactly
the existing numeric token-dispatch catchall documented in
`parser-policy.json`. The retained lexer is byte-identical (SHA-256
`ba451e10...`) to the lexer of the earlier scratch records, so this slice
does not audit its policy again.

## Checks

The [raw record](s0-5-r2-workload-parser.json) reports 114 exact syntax-tree
or refusal cases in 15 batches. All batches pass on Bun, the pinned Node
worker, and native (INFO), for 45 successful runtime batches. The harness
checks each generated Bend program before compiling it. Pin checks pass
before and after the run.

The suite keeps 72 of the 75 earlier cases unchanged. It changes three
earlier cases because the parser now accepts literals, `mu`, and `sum ()`.
The `numeric-term` case now expects a literal tree. The `unsupported-mu`
case becomes `mu-missing-body`, and `unsupported-sum` becomes
`nonempty-sum-deferred`. These two cases keep their input and fuel, but
they now expect an Unexpected error, not an Unsupported error. The suite
adds 39 cases: five complete workload trees, 14 focused positive
trees, and 20 malformed-input or exhaustion cases. The expected trees are
hand-authored independently of the parser in `workload-fixtures.py`.

The full-tree checks compare every declaration, constructor signature,
parameter, index, motive, arm, literal, and obligation body. They preserve
20 sole declarations in the core, 22 in the smoke workload, and 21 in each
negative fixture. These are sole declaration counts; the translation map
accounts for 19 original Bend core declarations. All three negative
fixtures intentionally parse. Their intended REFUSE verdicts remain open.

Bun detects all 17 compiled mutations: the eight earlier parser mutations
and nine new mutations covering constructor and parameter order, parameter
names, data result telescopes, indexed family/result separation, the
indexed motive binder, literal digits, empty-sum identity, and trailing
separators. Mutation checks run on Bun only. No mutation changes the plain
`as x return T` binder. The full-tree checks compare it.

The [artifact audit](s0-5-r2-workload-parser-audit.json) checks all referenced
hashes and byte counts, exact fixture coverage, endpoint commands and masks,
mutation coverage and source rewrites, and regenerated golden harnesses.
It also verifies that all eight retained source files match the inputs of
the successful development run byte for byte. That run is
`/private/tmp/sole-comb-step2-parser-20260927/validation-002/validation.json`.
The record binds the sources, bundles, commands, and each raw stdout and
stderr stream by path, byte count, and SHA-256 hash. The raw streams stay
in that run directory. An audit of the recorded run needs them.

`make gates` passed in a non-git copy of repository HEAD `d4006f2` with
the staged changes at
`/Users/oobi/Documents/gpt5/sole-comb-step2-parser-close/`: 143 regression
tests, 35 toolchain pin checks, and the Bend build check. The
[stdout](s0-5-r2-workload-parser-gates.stdout) and
[stderr](s0-5-r2-workload-parser-gates.stderr) retain the full results.
The README, manifest, record, audit, and review files were written into
the copy after that run. The gates do not read these files, and they do
not check the scratch parser.

## Reproduction and next step

Use a new output directory:

```sh
python3 -P /private/tmp/claude/kan-elim-lang-m0/r2-risk/workload-parser-v2/validate-workload-parser.py \
  --output /private/tmp/sole-comb-workload-parser-rerun --mutations
```

To audit either that run or the recorded run, invoke
`audit-workload-parser.py` from the same source directory with `--record`
naming its `validation.json`, `--repo /Users/oobi/Documents/sole-comb`, and
`--output` naming a new JSON file. The audit checks freshness and consistency
of evidence; the runtime checks supply the parser validation.

Step 2 remains open. The next slice must meet all step 2 requirements in
the [S0-5 closeout](../s0-5-closeout.md) and the
[translation review](s0-5-r2-twins.md). These requirements include the
zero-arm elimination of `sum ()` and constructor checking. The checker must
compute the ACCEPT and REFUSE verdicts from source. The first task is to
resolve the new syntax and derive checked data metadata from source.
The older resolver and data checker still target the earlier syntax and
caller metadata. They are not connected to this parser extension. Portable
input, command qualification, startup measurements, timing samples, and
`dev/r2-risk.json` also remain pending.
