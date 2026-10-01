# Stage A.5b.3.2d: public dependent-pair erasure

`./sole-comb check --erased examples/pair-erasure.sole-comb` erases dependent
pairs. Native elimination uses exactly one inline two-binder lambda:

```text
elim p { fun (x : Nat) (y : Nat) => x }
```

The first binder's quantity matches the pair type. The fibre annotation opens
under that binder, allowing `fun (0 A : Type 0) (value : A) => (A, value)`.
Optional `as self return Type` motives support results needed for inference.
Malformed arm counts, binder counts, annotations, results and quantity use
receive checking refusals. Named pair arms and defaults remain unsupported.
The reserved `.1` and `.2` lexical forms remain refusal cases.

Layouts use `pair<...>` and omit type, proof and zero-quantity point fields.
Fibre erasure uses the actual point value; layout computation opens the fibre
at a fresh variable. A fully erased pair becomes `KErased`. Pair elimination
binds the scrutinee once, then projects each retained field. The synthetic
runtime slot does not occupy a kernel index. Lifted closures omit that slot
and retain only their live kernel captures.

`erase/pair.bend` plans pair introductions and eliminations, `erase/type.bend`
derives layouts, and `erase/core.bend` uses its existing bounded trampoline.
The port follows clean Kanon revision
`69f3be5198cda4334de4fd23b4ccc92cac595789`, especially `pair_texts`,
`pair_intro` and `pair_elim` in `lib/erase.ml`. As with the sum plans, the
elimination plan computes its layout before erasing the scrutinee; competing
refusal ordering can differ after a checking failure.

The existing core line limit remains 660. The type and surface elaboration
limits grow to 230 and 640, and the new pair planner has a 90-line limit.
One callback termination exemption delegates to the fuel-spending type
representation visitor. One surface catch-all explicitly refuses an arm
without its fibre lambda. Type, affine-use, purity, exhaustive-match and
production import-closure checks remain enabled.

The public census is 14 positive examples and 54 refusals. Erasure validation
compares eight public examples with fresh pinned Kanon output, including a
33-declaration pair fixture whose native eliminations match pinned projections.
Seven independent pair goldens cover field retention and complete erasure.
Three additional native-elim goldens (the lifted closure, its caller and a
repack) cover a closure capturing both fields and an outer variable, and
reconstruction of an existential pair. Direct contracts check malformed pair
plans, synthetic slot lookup, capture frames and closures.
Four new isolated mutations target fibre offsets, the synthetic slot, capture
framing and fibre instantiation; the full mutation suite contains 18 cases.
All 18 were rejected semantically in one full run of the staged harness. The
result is recorded in `stage-a-pair-erasure-mutations.json` with
`stage-a-pair-erasure-focused.stdout.log` and
`stage-a-pair-erasure-focused.stderr.log`. During development, the initial
fibre candidate caused an erasure budget failure and an intermediate candidate
failed affine-use checking. Neither was counted. The final candidate substitutes
the point's type for its value when opening the fibre. The earlier attempt logs
stay as history in the four `stage-a-pair-erasure-mutations-{initial,retry}.*`
logs.

Bun, Node worker and native public, oracle, golden and direct contract checks
passed in the focused run. The three-host public suite checked 68 files with
217 observations. The source policy passed with 49 files, 38 line budgets, 57
reviewed termination exemptions and 30 catch-all entries.

During development, the public native compiler exited with signal 9. Two full
gate attempts stopped at the unchanged A.2 representation native build's
120-second timeout; only the retry is preserved. The retry passed the
foundation checks, the Bun and Node representation checks, and the Python
development checks before reaching that timeout. Native build deadlines and
the default host requirements remain unchanged.

The checked source hashes and host observations are recorded in
`stage-a-pair-erasure.json` and `public-pair-erasure.json`. The incomplete gate
retry is preserved in `stage-a-pair-erasure-gates.stdout.log` and
`stage-a-pair-erasure-gates.stderr.log`. The earlier public native build
failure is preserved in `stage-a-pair-erasure-native.stderr.log`.

This extends [finite-sum erasure](stage-a-sum-erasure.md). Recursive families,
recursive definitions, complete erased-corpus integration and WebAssembly
build/run remain pending.
