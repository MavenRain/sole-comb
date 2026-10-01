# Stage A.5b.3.2b: public product erasure

`./sole-comb check --erased examples/product-erasure.sole-comb` now erases
collection tuples and closed records, including their generated getters.
Type and proof fields drop before runtime field numbering. Layout names retain
the pinned field order and representations of nested products and functions.
Empty tuples drop, and nonempty tuples with no runtime fields produce `KErased`.
Lifted closures preserve captures inside tuple fields.

Declaration groups collect layout names from function bodies as well as
signatures. The new `erase/dependencies.bend` module performs this traversal
in the pinned first-mention order before the existing deduplication step.

This extends [ordinary erasure](stage-a-core-erasure.md). Sum layouts and
elimination, recursive runtime definitions, full erased-corpus integration,
and WebAssembly build/run remain pending. Applications whose remaining
parameters all erase still report the existing explicit refusal. A.5b.3.2
and A.close remain open.

The production files have 581 lines in `erase/core.bend`, 167 in
`erase/type.bend`, and 30 in `erase/dependencies.bend`. Their scoped line
limits are 600, 190, and 45. The two existing limits grow only to accommodate
this increment; the new module receives its own bound. Six new termination
exemptions have individual reasons in `dev/bend-policy.json`. Nested type
representation spends finite fuel. Type, affine-use, exhaustive-match, purity,
and production import-closure checks remain enabled.

## Validation

The [core record](stage-a-product-erasure.json) binds 55 input hashes and
21 public erasure observations across all three hosts. The
[public record](public-product-erasure.json) covers 55 native files and
178 observations. The [mutation record](stage-a-product-erasure-mutations.json)
records eight semantic rejections. The focused command
`make test-cli test-core-erasure-mutations` exited zero; its
[stdout](stage-a-product-erasure-focused.stdout.log) and
[stderr](stage-a-product-erasure-focused.stderr.log) are saved.
Validation ran in an isolated copy with the repository's Git metadata.

The differential harness compiles a fresh oracle from clean Kanon revision
`69f3be5198cda4334de4fd23b4ccc92cac595789`. It compares every copied source
with the revision's Git blob before compilation. The new oracle input is
the independently written product expansion in `test/product-erasure.kan`.
The pin lacks record syntax and qualified surface names, so the fixture uses
unqualified getter names. A fixed mapping restores those names in the oracle
output; the saved record discloses both the input override and name mapping.

Four public examples compare with the fresh oracle on Bun, Node worker, and
native. The product example has 40 checked declarations. Seven fixed product
goldens additionally constrain proof removal, runtime projection offsets,
nested layouts, entirely erased tuples, and body layout dependencies.
Seven direct contracts include exhausted term and representation fuel.

Eight isolated semantic mutations cover retained zero parameters, retained
proofs, shifted variable indices, unpruned captures, reversed tuple fields,
unfiltered projection offsets, malformed layout names, and omitted body
layout dependencies. Compilation failures do not count as semantic rejection.

The public example census includes `product-erasure.sole-comb` with its exact
40-declaration expectation. Existing record admission and sum erasure refusal
cases remain in the suites.

The complete `make gates` run remains incomplete. Two attempts stopped at
unchanged native compilation deadlines of 120 seconds under high machine load:
first foundation, then representation after foundation passed on all hosts.
The latter [stdout](stage-a-product-erasure-gates.stdout.log) and
[stderr](stage-a-product-erasure-gates.stderr.log) preserve this result.
Representation's 465 cases passed on both JS hosts. These deadlines were
preserved. The successful focused native comparisons above cover this
increment; they do not establish completion of the full gate.

The final pin check `python3 -P dev/pin-check.py` saves its output in
[stdout](stage-a-product-erasure-pincheck.stdout.log). The production
build check also passed; its [stdout](stage-a-product-erasure-build.stdout.log)
is saved.
