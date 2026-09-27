# S0-5 scratch function-fragment conversion

The scratch checker now normalizes resolved function terms and compares
their normal forms. On 2026-09-27 UTC, 107 exact-normal-form, comparison,
and refusal cases passed on Bun, the pinned Node worker, and the
informational native C build. Twenty deliberately incorrect variants built
successfully and were rejected by their tests on Bun.

The compiler code remains outside this repository, as required by S0-5:

```text
/private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/
  conversion.bend
  conversion-policy.json
  validate-conversion.py
  conversion-validation-003/
```

The [validation record](s0-5-r2-conversion.json) is a byte copy of that
directory's `validation.json`. It binds the implementation, validator and
dependencies, policy, fixtures, generated tests, source copies, bundles,
and command logs to their hashes. Both endpoint selection and the R2
verdict are null. These are correctness checks, not benchmark samples or
probe qualification.

## Implemented fragment

`normalize(fuel, depth, globals, term)` returns an explicit error or a
normal form. `convert(fuel, depth, globals, left, right)` normalizes each
operand and compares the results. Both operate on the resolver's terms.

The supported reductions are beta application, delta unfolding of explicit
definitions, zeta reduction of `let`, and ascription erasure. Normal forms
erase binder names and lambda annotations. Dependent function domains and
codomains normalize under their proper scopes. Closures retain local
environments; quotation introduces fresh de Bruijn levels and converts
them back to indices, avoiding variable capture.

Universe levels are decimal strings. Leading zeros are removed without a
bounded integer conversion, and empty or nondecimal levels are refused.
Universe equality is exact; this is not a cumulativity check.

The caller supplies the number of open locals and a global context of
opaque constants or closed, resolved definitions. Opaque names remain
neutral. Definition bodies evaluate in an empty local environment. The
engine does not check that global names are unique. If a name repeats, the
first matching binding wins. Direct API tests show that cycles exhaust fuel
and unbound names fail.

The source resolver enforces ordered, nonrecursive, unique definitions, but
the engine does not receive the resolver declarations as its context. No
pass builds the global context from resolver declarations yet. Each source
case resolves one definition and normalizes its body against the default
opaque context, so the source cases do not exercise delta through the
resolver.

`Prop` is not a sort in this pass. It reaches the engine as
`Global("Prop")` only when the caller lists it, and the tests list it as an
opaque constant. So `Prop` is equal only to itself, and `Prop A` stays
neutral instead of returning `InvalidApplication`.

There are no unsafe declarations or catchall patterns. One unit of fuel
pays for one normalization-machine transition, including environment
initialization. A local or global lookup costs one unit for each
environment slot that it walks, not one unit for each lookup. Read-back of
a local also costs one unit for each level that it walks, so a local at
depth d costs the same for every index. Conversion gives each operand its
own budget. Structural equality and string/number helpers are outside that
budget, so fuel is not a CPU or memory limit. Exhaustion is an error and
never reports inequality or partial success.

## Validation

Reproduce in a new output directory:

```sh
python3 -P /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/validate-conversion.py \
  --output /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/conversion-validation-004 \
  --mutations
```

The recorded run came from this command, with the output directory listed
above. The validator refuses an existing output directory.

The 107 cases cover open and closed terms, capture avoidance, shadowing,
neutral application spines, dependent functions, definition chains,
annotation erasure, decimal universe levels, invalid applications, explicit
refusals, and exact/insufficient/surplus fuel. They also cover deep-local
comparison, four cross-constructor pairs in both orders, per-operand
conversion fuel, exact and one-short lookup fuel, and duplicate global
names. Five cases run source text through the existing lexer, parser, and
resolver before normalization.

The twenty mutations target captured environments, fresh lambda and Pi
levels, quoted indices, closed global scope, application order, universe
and Pi comparison, exhaustion, global lookup, projection refusal, `let`
binding, local comparison, the two Global/Apply comparison arms, the right
operand's fuel, local and global lookup charges, the read-back charge, and
first-match global lookup. Every mutant passed Bend checking and
JavaScript generation; its selected fixture then returned the expected
failure mask.

`make gates` also passed against the executable sources from `c3db16c` in
`/Users/oobi/Documents/gpt5/sole-comb-conversion-close/`. The six suites
passed 143 tests in total, followed by pin checks and the host entry check.
Full [stdout](s0-5-r2-conversion-gates.stdout) and
[stderr](s0-5-r2-conversion-gates.stderr) are retained.

## Remaining boundary

This is a strict normalizer for the function fragment, not a typechecker.
Conversion is untyped. Annotations are erased without being checked. Equal
normal forms show alpha/beta/delta/zeta convertibility only. The engine
does not apply eta or proof irrelevance, so it can report False where typed
conversion reports True. Evaluated projections and eliminations return
`Unsupported`; records, `else` expansion, data reduction rules, typing, and
the sort meaning of `Prop` that the [resolver record](s0-5-r2-resolver.md)
defers are still pending. The two twins, portable file input, correctness
qualification, runtime measurements, and endpoint selection remain open.

## Next step

Implement scratch typing first. Then add sugar expansion and the data and
projection reductions. After that, connect portable file input and choose
the two source twins. Prepare the twins and qualify them before any runtime
measurement.
