# Stage A.5b.3.2e.7: public family default arms

This increment adds a final `else: term` arm to native family elimination.
The family must already be admitted, and the existing motive requirements
apply. Expansion uses checked constructor metadata from that family.

For `mu B : Type 0 { a : B; b : B; c : B }`,
`elim v as x in B return Nat { 1; else: 2 }` supplies the arms for `b` and `c`.
It has the same checked and erased output as `{ 1; 2; 2 }`.
Explicit arms keep their constructor addresses. A default covers only
unhandled constructors, in declaration order, and must cover at least one.
An empty family and an exhausted constructor list refuse a default. An
out-of-range explicit arm is refused before a following default is expanded.
The parser requires the default to be final; the expansion helper also checks
this condition for callers that already hold syntax values.

Every generated arm goes through `surface/family.bend`'s existing constructor
field decomposition and `surface/elab.bend`'s existing branch checking.
For a constructor with fields, the default must bind those fields with lambdas.
Field types and quantities are checked for each constructor. Remaining lambda
binders form the branch result, so a nullary constructor can return a function
and a constructor with fields can preserve a function-valued result.
One default value must fit every remaining constructor. If a copy does not
fit, the checker refuses it with the same error as the equivalent explicit
arm. The error does not identify the default as its source.
The expansion does not create field names or insert untyped core terms.
Parameters, dependent field types, result indices and captures continue through
the ordinary kernel-checked arm path.

The host module remains structurally terminating without new unsafe functions
or catch-all rows. Its line budget grows from 130 to 150 for the expansion
helpers; the final module has 147 lines. The policy still registers 63 unsafe
sites and 41 catch-all functions across 54 host files and 43 line budgets.

## Evidence

The native examples are `examples/family-default-arms.sole-comb` and its
independently written explicit expansion. Their eleven definitions cover nullary
constructors, suffix coverage, multiple fields, linear fields, dependent fields,
captures, function results with and without constructor fields, parameterized
families and indexed families.
Both `check --print` and `check --erased` must agree exactly with the explicit
expansion and across Bun, the Node worker and native.

`test/family-default-order.sole-comb` has a hand-derived erased expectation for
three constructor addresses and two literal branch-body patterns. It checks
addresses and literal bodies only. The kernel keys each branch by its
constructor address, so the order of generated arms is not observable.
Nine refusal cases check an exhausted default, an empty family, missing fields, an invalid
field annotation, an invalid result, discarded linear fields, runtime use of
ghost fields, an out-of-range explicit arm and a nonfinal default.
The dedicated suite records 69 observations across all three hosts.

Four isolated mutations test remaining-constructor tracking, complete coverage,
default-body preservation and the refusal of a default with no remaining
constructor. A host compile failure cannot count as a semantic kill. The
existing public-family suite retains its four mutation obligations, which
include field annotations. `make check`, `make test` and `make gates` include the
new suite; the latter two require its mutations.

## Validation

Validation ran on the working copy based on
`1f5eeb8dec2de75dea528ff98b522a86cbde4e9c`. The new suite passed all 69
observations on Bun, Node worker and native, plus four semantic mutations.
The public CLI passed 103 inputs and 322 observations. The existing public-family
suite passed its 16 independent goldens and four semantic mutations.

`make gates` ran without `-o` flags on a scratch copy of the staged tree that
includes the review fixes, and exited 0 after 81 minutes. It ran every target of
the Makefile `gates` line: the regression, house policy, reference fixture,
foundation, representation, evaluation, checking, parser, elaboration,
whole-program, erasure-support, public CLI, core-erasure, family layout, family
erasure, public-family, constructor parameter, constructor inference and family
default gates. Then it passed all 34 pin checks and the compiler check.

The saved records are `stage-a-family-defaults.json`,
`stage-a-family-defaults-cli.json` and
`stage-a-family-defaults-public-family.json`. They include source hashes;
all 365 hashes were checked against the staged files. The gate streams are
saved in `stage-a-family-defaults-gates.{stdout,stderr}.log`. They show the
paths of the scratch copy.

## Remaining boundary

Structural recursors still need the K1 kernel changes that supply induction
hypotheses, evaluate recursor iota steps and erase those hypotheses. Recursive
definitions, complete erased-corpus integration, WebAssembly build/run and the
full kernel differential remain pending. Family defaults expand through the
existing nonrecursive family eliminator and supply no induction hypotheses.
UAT revision pinning, theorem selection and the transitive axiom inventory also
remain pending.
