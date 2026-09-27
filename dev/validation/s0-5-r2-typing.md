# S0-5 scratch function-fragment typing

The scratch typechecker passes 96 exact signature and refusal cases in
twelve batches on Bun, the pinned Node worker, and the informational native
build. All nineteen semantic mutations are detected by their selected Bun
fixtures after passing Bend checking and JavaScript generation. The full
[validation record](s0-5-r2-typing.json) binds source files, fixtures,
generated tests and bundles, and command logs by hash.

This is frontend development evidence. No twin has been qualified, no R2
measurement has been taken, and no runtime endpoint has been selected.

## Scratch artifacts

The implementation stays outside the repository, as required by S0-5:

```text
/private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/
  typing-values.bend
  typing.bend
  typing-policy.json
  typing-fixtures.py
  validate-typing.py
  audit-typing-record.py
  typing-validation-005/
```

The implementation reuses the validated lexer, parser, resolver, and
conversion engine without changing their sources. The source entry point
`parse_check` gives each frontend pass an explicit fuel budget and supplies
only the built-in `Prop` to the resolver. The lower-level `program` entry
checks resolved declarations. Internal helpers require well-formed typed
contexts; they are not interfaces for accepting untrusted context entries.

## Checked fragment

The checker synthesizes types for variables, dependent products,
applications, annotated lambdas, ascriptions, and `let` expressions.
Unannotated lambdas require an expected product type. Every annotation must
itself have a sort. Values and bodies are checked before their bindings are
available to later declarations. Duplicate global names, including `Prop`,
are rejected, and definitions cannot refer to themselves or later names.
An unused `let` value is still checked.

Product and lambda values keep the local environment of their source term,
so a dependent codomain or body reads the correct earlier locals.
Type comparison quotes both values and compares their alpha/beta/delta/zeta
normal forms. The checker uses exact universe equality, with no universe
or `Prop` cumulativity. Decimal levels have arbitrary length: `Type n` has
type `Type (n+1)`, and type-valued products use the maximum domain/codomain
sort level. A propositional domain sort contributes level zero. `Prop`
itself has type `Type 0`; a product whose codomain has sort `Prop` also has
sort `Prop`, even when its domain is in a higher universe.

The two new Bend modules, the conversion engine, and the resolver contain
no unsafe declarations or catchall patterns. The lexer and parser keep six
recorded numeric fallback arms: five in the lexer, listed in
`bend-policy.json`, and one in the parser, listed in `parser-policy.json`.
The validator checks the dependencies against that recorded allowlist.
Typing runs through an explicit request interpreter: each request, binding,
and return transition consumes fuel. The program loop uses one unit before
each declaration and one unit after the last declaration. Declaration `k`,
counted from zero, gets the typing fuel minus `k+1`. The duplicate-name
scan, the annotation sort check, the annotation evaluation, the value
check, and the annotation read-back each start from that declaration
budget. Inside the interpreter, evaluation, read-back, and local/global
lookup receive separate copies of the remaining budget. Structural
equality, decimal arithmetic, normal-form embedding, and context
construction are outside that budget. Fuel is not a global work, CPU, or
memory limit. Exhaustion returns an error and never publishes a partial
declaration list as success.

## Validation

The cases exercise dependent checking and synthesis, binder shadowing,
capture-safe application, closed global types, checked annotations and
discarded bindings, arbitrary decimal levels, product-sort maxima,
impredicative `Prop`, declaration order, and explicit unsupported forms.
They check exact error constructors, including errors propagated from the
lexer, parser, resolver, and normalizer. Bare-lambda inference, eta, proof
irrelevance, and cumulativity have explicit refusal cases.

The mutations change universe successor, conversion mismatch, product-sort
maximum, `Prop` impredicativity, argument checking, lambda annotations,
`let` value checking, ascriptions, local and global lookup, duplicate names,
fuel exhaustion, inferred closure environments, fresh binder levels, the
level that a propositional domain sort contributes to a product, the sort
check of an inferred lambda annotation, the codomain environment in
application and in checking, and the digit order of the universe maximum.

Reproduce in a new directory:

```sh
python3 -P /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/validate-typing.py \
  --output /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/typing-validation-006 \
  --mutations
python3 -P /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/audit-typing-record.py \
  /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/typing-validation-006/validation.json
```

The recorded run used `typing-validation-005`; the validator refuses an
existing output directory. The audit rehashes the referenced artifacts and
checks completion, the full endpoint/case matrix, generated artifacts,
mutation results, successful Bend checks, and before/after pin checks.

`make gates` passed against the executable sources from `8c9bea6` in
`/Users/oobi/Documents/gpt5/sole-comb-typing-close/`: 143 tests across six
suites, pin checks, and the host entry check. Full
[stdout](s0-5-r2-typing-gates.stdout) and
[stderr](s0-5-r2-typing-gates.stderr) are retained.

## Remaining boundary and next step

This is the scratch function fragment, not the Stage A kernel. Conversion
still has no eta or proof-irrelevance rules and can conservatively refuse
programs that a full typed conversion engine would accept. Projection and
elimination are explicit `Unsupported` errors; record and `else` sugar,
data typing and reduction rules remain pending.

Next, implement scratch sugar expansion and the data/projection rules
needed by the two source twins. Then add portable file input, choose and
qualify the twins, and collect the required measurements. Typing fixtures
do not qualify an endpoint or establish an R2 verdict.
