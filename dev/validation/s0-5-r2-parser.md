# S0-5 scratch parser

The scratch probe now parses a documented sole-comb subset into explicit
syntax trees. It does not resolve names, expand sugar, typecheck programs,
qualify an R2 endpoint, or supply a performance verdict.

The implementation stays outside the repository, as required by M0-PLAN S0-5:

```text
/private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/parser.bend
/private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/parser-policy.json
/private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/validate-parser.py
```

It imports the previously validated `lexer.bend`. The validator reuses process
and launcher helpers from the scratch `validate.py`. The
[validation record](s0-5-r2-parser.json) binds those sources, the policy,
toolchain file, launcher implementation, pin checker, golden fixtures,
generated test sources, bundles, commands, and full logs to their hashes.
All 263 unique file references were rehashed successfully before staging.
The scratch directory is required to rerun this evidence. These files are
not part of the Stage A kernel or a portable source distribution.

## Parser contract

`parse(lex_fuel, parse_fuel, text)` returns a complete declaration list or a
typed error. The lexer retains its Latin-1 byte carrier, byte offsets, and
one-based line/column contract. Parser errors retain the current token's
location. Lexical errors remain distinct from unexpected syntax, unsupported
keywords, forbidden pattern keywords, and parser exhaustion.

The subset supports:

- `def name : type := value` and `record Name { field : type, ... }`.
- Names, `Prop`, `Type` with an optional decimal universe level, and grouping.
- Application, right-associative arrows, and `(name : type) -> body` binders.
  The group binds a name only when it holds one identifier. Any other
  annotated group, such as `(Prop : Type)` or `((y) : A)`, is an ascription
  in a non-dependent arrow.
- `fun name ... => body`, including annotated `(name : type)` parameters.
- `let name : type := value in body`, `(value : type)`, and named projections.
- `elim value { arm; ... }`, optional `as name return type` motives,
  optional `Ctor:` labels, and explicit `else:` arms. Empty arm lists are
  represented for the zero-constructor case.

Application associates left. Projections bind tighter than application,
which binds tighter than arrows. Arm, field, binder, and declaration order
is preserved. Labels never reorder arms. Arms are ordinary terms, including
names and lambdas; pattern binders are refused. `match` and `case` produce
`Forbidden` errors in the tested term, binder, declaration, field, and label
positions. Trailing commas and semicolons inside records and eliminators
are refused.

This is a syntax boundary. Fields and names remain symbolic; a later pass
must resolve projections to indices. The checker must enforce label order,
motive requirements, duplicate-name rules, typing, and conversion. `else`
expansion is still pending. Bare numerals, mu declarations, recursive
definitions, axioms, and the remaining full-language forms are outside this
subset. A parsed `Prop`, universe, or unresolved name is not a typing verdict.

The parser uses an explicit state machine. Every transition spends one unit
of parser fuel, separately from the lexer's byte budget. Exhaustion returns
an error, never a partial declaration list. The implementation has no
`@unsafe` declarations. All datatype matches are exhaustive; the single
numeric token-dispatch fallback is listed in `parser-policy.json`.

## Validation

All 75 cases pass on Bun, the pinned Node worker, and the informational native
executable. Every generated test module passes Bend check-only validation
before either backend is built. Pin checks before and after the run pass
all 35 checks.

Golden assertions compare complete AST constructors, fields, and list order,
or the exact error variant and byte/line/column location. Cases include
precedence, nested lets and eliminators, typed lambdas, record projections,
labels and motives, CRLF and comments, malformed syntax, unsupported forms,
lexical errors, exact, surplus, and insufficient fuel, a 128-argument
application, and 128 nested parentheses.

Eight separately checked and compiled mutations are detected under Bun:
swapped application operands, left-associated arrows, reversed declarations,
reversed arms, `match` accepted as a name, exhaustion changed to success,
an annotated argument consuming an outer arrow, and a projection that does
not bind to an application argument.

The repository's `make gates` passes: 143 regression tests (26 benchmark,
4 build, 3 pin-check, 40 evidence, 32 runner, and 38 preparation), all pin
checks, and the host entry check. Full [stdout](s0-5-r2-parser-gates.stdout)
and [stderr](s0-5-r2-parser-gates.stderr) are retained alongside this record.
An initial sandboxed run passed the tests but could not write the host
check log; the saved logs are from the subsequent successful complete run.

To rerun without overwriting evidence, choose a new output directory:

```sh
python3 -P /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/validate-parser.py \
  --output /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/parser-validation-next \
  --mutations
```

## Next step

Implement the scratch checker and portable file-input boundary, choose the
small and conversion-heavy source twins, and extend the parser subset if
their syntax requires it. After semantic and rejection checks pass, use the
existing preparer and serial measurement runner. Parser fixtures are not
qualified probe workloads. The pinned endpoint remains null, and no R2
verdict has been recorded.
