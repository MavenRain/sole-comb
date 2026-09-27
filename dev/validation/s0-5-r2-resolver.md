# S0-5 scratch name resolver

The first scratch checker pass resolves names in the parser's core subset.
It passed 76 exact-tree and refusal cases on Bun, the pinned Node worker,
and the informational native C build on 2026-09-27 UTC. Ten deliberately
incorrect resolvers built successfully and were rejected by their tests.

The compiler code remains outside this repository, as required by S0-5:

```text
/private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/
  resolver.bend
  resolver-policy.json
  validate-resolver.py
  resolver-validation-003/
```

The [validation record](s0-5-r2-resolver.json) binds the source files,
validator dependencies, policy, fixtures, generated tests, source copies,
bundles, and command logs to their hashes. Its endpoint and R2 verdict are
null. This is frontend correctness evidence and does not qualify a probe
workload or select a runtime.

## Resolution contract

`resolve(fuel, globals, declarations)` accepts parsed declarations and an
explicit list of available global names. `parse_resolve(lex_fuel,
parse_fuel, resolve_fuel, globals, text)` connects the existing lexer and
parser to this pass. Both return a complete resolved declaration list or a
typed error. Lexical and parser failures retain their original error and
location inside `Syntax`. `Unknown` and `Duplicate` carry the name, and
`NeedsSugar` carries the form (`record` or `else`). `AnonymousDefinition`,
`Exhausted`, and `InvalidState` carry no data. Resolution errors carry no
source location, so an exhausted run does not identify the definition that
it reached.

- Local variables become unbounded natural-number de Bruijn indices, with
  zero naming the nearest binder. Local bindings take precedence over globals.
- Anonymous binders occupy a slot and cannot be referenced by `_`.
- Definitions are ordered and nonrecursive. An annotation and value see
  only the explicit initial globals and earlier definitions. Duplicate names,
  including collisions with initial globals, and anonymous definitions fail.
- Lambda annotations and Pi domains use the outer scope. Bodies and Pi
  codomains include their binder. Let annotations and values use the outer
  scope; only the body sees the let binder.
- An elimination motive's binder scopes only its return type. Arms use the
  outer scope, with any explicit lambdas introducing their own binders.
  Arm labels and order are preserved, and sibling scopes remain separate.
- Projection field names and constructor labels remain symbolic for later
  type-directed checking. Their existence and validity are not checked here.
- Record declarations and `else` arms return `NeedsSugar`. Sugar expansion
  must precede their resolution. Universes retain their parsed decimal text.
- There are no implicit prelude names. A successful result establishes
  scope correctness, not typing, conversion, or universe correctness.
- The parser reserves `Prop`, so no definition or binder can use it as a
  name. It reaches the resolver as a name and resolves only as a global. It
  becomes `Global("Prop")` when the caller lists it in the initial globals;
  otherwise it fails as `Unknown`. A later pass gives it its meaning as a
  sort.

The machine has no unsafe declarations or catch-all patterns. Each transition
consumes one resolver fuel unit. Context lookup, string comparison, and list
reversal use structural recursion. Fuel bounds transitions, not CPU time.
Lexer and parser fuel are independent, and exhaustion cannot return a partial
program. An empty program needs one resolver unit; a simple global-reference
definition needs six. Tests exercise both exact boundaries and refusal one
unit short, along with 64 nested binders.

## Validation

Reproduce in a new output directory:

```sh
python3 -P /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/validate-resolver.py \
  --output /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/resolver-validation-004 \
  --mutations
```

The validator runs the repository pin checks before and after validation.
All ten batches pass Bend `--check-only`, JavaScript compilation, Bun and
Node worker execution, native compilation, and native execution. Every
endpoint passes the same 76 cases. Each test compares the complete result
against an explicit expected tree or error, including indices and metadata.

The ten detected mutations cover local indices, duplicate globals, lambda
annotation scope, let-value scope, motive scope, anonymous binder depth,
declaration order, arm order, exhaustion, and `else` arms after the first
arm. Every mutant separately passes the Bend checker and JavaScript build
before its Bun test reports the expected nonzero failure mask.

`make gates` also passes in a clean checkout of `ea57530`, with the same
tracked executable sources as the sole-comb repository. The
[stdout](s0-5-r2-resolver-gates.stdout) and
[stderr](s0-5-r2-resolver-gates.stderr) preserve that gate run. No timing
measurement was taken, so the benchmark load ceiling does not apply.

## Next step

Choose and record the two source twins. Implement scratch sugar expansion,
typing, and conversion for the constructs that the twins need, then connect
portable file input. Qualify the two twins through the probe preparer before
collecting endpoint, denominator, and startup measurements. The Stage A
kernel port and runtime selection remain pending.
