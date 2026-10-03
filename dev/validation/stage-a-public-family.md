# Stage A.5b.3.2e.3: public recursive families

The public source checker admits `mu Name (parameters) : type { constructors }`.
Constructors use `name : type`. Constructors are separated by semicolons, and a
trailing semicolon is allowed. Empty declarations use `{}`. Mutual groups use
`and Name (parameters) : type { constructors }`. Constructor order determines
the tags and the positions of native `elim` arms. For example:

```text
mu N : Type 0 { zero : N; succ : N -> N }
def read : N -> Nat := fun (n : N) =>
  elim n as x in N return Nat { 1; fun (m : N) => 2 }
```

Family elimination requires an explicit motive. A nullary constructor's arm is
its result expression. Other arms bind every constructor field with typed
lambdas, including proof and zero-quantity fields. The expander consumes exactly
the checked constructor telescope and preserves any remaining function result.
It also handles nested lambda groups. The surface elaborator checks each field
annotation against the declared field type. The kernel checks quantities, affine
use, branch targets and complete constructor coverage.

The parser refuses a constructor name that occurs two times in one family or in
one `and` group. It also refuses a constructor that has the name of its own
family or of an earlier family in its group. These refusals give the source
location of the constructor name. The elaborator refuses a constructor that has
the name of a later family in its group, and a constructor name that an earlier
declaration uses as a family or constructor name. This guard runs when the
group is declared, and its refusal has no source location. A family name that an earlier
constructor uses is not refused.

The first version of this guard looked up the earlier families and
constructors inside nested closures, after the group lookup. Its native
executable admitted these constructors, but Bun and Node worker refused them.
The guard now gives each lookup result directly to `Bool.or` and does the
group lookup last. The cause in the native compiler is not known.
`family-ctor-redeclared` and `family-ctor-earlier-family`
cover the two earlier-declaration lookups on all three hosts.

`surface/family.bend` implements the brace parser and positional arm expansion.
Both closing-brace exits of the constructor parser use one helper that restores
the declaration order.

The public-family harness checks the complete erased output of
`examples/families.sole-comb` exactly on each selected host: Bun, Node worker or
native. It also checks 16 independent literal goldens. Each golden is a line that
must appear in the frozen oracle. The goldens are chosen per feature: constructor
tags, nested constructors, positional arms, field positions, erased fields and
runtime placeholders, empty matches, mutual groups, captured variables and
function-valued results.

The harness also checks `test/public-family-order.sole-comb` exactly on each
selected host. This probe declares a family with a trailing semicolon and two
nullary constructors whose arms give different results. It also declares a
constructor with two fields of the same type and quantity, and its arm returns
the first field. The expected output is literal text in the harness. This text
was derived by hand from the erasure rules, because the probe has no Kanon
oracle.

The CLI census has 90 examples and refusal cases. Twenty-one new family refusals
cover malformed declaration headers and delimiters, legacy syntax, positivity,
duplicate constructor names, wrong constructor results, missing and excess arms,
field arity, annotations, quantities, affine duplication, wrong arm results,
unsupported defaults, parameterized-constructor inference and missing motives.
The review added five of them: `family-header`, `family-group-duplicate`,
`family-ctor-family-name`, `family-ctor-redeclared` and
`family-ctor-earlier-family`. The default-arm refusal
has an explicit motive, so only its default arm is wrong. The earlier incomplete
family declaration now reaches its missing-brace refusal.

Four isolated mutations reverse constructor order, reverse field order, discard
a remaining result lambda and drop field annotations. The constructor-order
mutation changes the single helper that restores constructor order. Every mutant
must compile successfully. Build failures and host failures do not count as
kills. The record gives each mutant one `kill` kind:

- `output-changed`: exit 0, empty stderr, and stdout that differs from the
  expected output.
- `refused`: exit 1, empty stdout, and stderr that starts with
  `CHECK <source> FAIL`.
- `admitted`: exit 0, empty stderr, and stdout that starts with
  `CHECK <fixture> defs=1 ok` for a refusal fixture.

The constructor-order and field-order mutants run on the probe and accept only
`output-changed`. The result-lambda mutant runs on `examples/families.sole-comb`
and accepts `output-changed` or `refused`. A refusal of this well-typed program
is the expected observation for this mutant, because the checker then sees an
arm result at the wrong type. The field-annotation mutant runs on the
wrong-domain fixture and accepts only `admitted`. All observations and source
hashes are saved in `_build/public-family/result.json`.

The focused, mutation and regression runs use `--hosts bun,node-worker`. The CLI
run uses the default host list. The `make test-public-family`
and `make test-public-family-mutations` targets use the default host list, which
includes native. The differential and the order probe passed on Bun and Node
worker. The four mutations ran on Bun, with these kill kinds: constructor-order
(`output-changed`), field-order (`output-changed`), result-lambda (`refused`)
and field-annotation (`admitted`). The 90-case public CLI checks passed on Bun,
Node worker and native. One native build at the tracked limits passed.
`python3 -P dev/test-public-family.py --hosts native` then passed with the exact
erased output, the literal goldens and the order probe.
The tracked deadlines and the default host list are unchanged. `check`, `test`
and `gates` now include the public-family targets. The full `make gates` has
not been rerun on this tree.

Before the review fixes, the checkout
`/Users/oobi/Documents/gpt5/sole-comb-public-family` ran a full `make gates`. It
stopped when the unchanged foundation native compilation exceeded its tracked
120-second limit. Its resulting executable then printed `PASS` for its 275 cases.
The command and log of that direct run were not retained. The affected target
run reached public native compilation, where the compiler was killed with signal
9 after type checking succeeded. The log gives no reason for the kill. These
runs are historical evidence for the earlier tree.

The ordinary, product, sum and pair erasure regressions passed on Bun and Node
worker: eight exact oracle comparisons, three pair goldens, one refusal and the
direct contracts. The public compiler's JavaScript check also passed. The
regression record keeps the A.5b.3.2d `milestone` and scope labels that
`dev/test-core-erasure.py` writes.

`stage-a-public-family.json`, `stage-a-public-family-cli.json` and
`stage-a-public-family-erasure-regressions.json` are copies of
`_build/public-family/result.json`, `_build/public/result.json` and
`_build/core-erasure/result.json` from these runs. They retain the exact
observations and source hashes. The family record hashes 134 sources, the CLI
record hashes 140 sources and the regression record hashes 68 sources.
`stage-a-public-family-execution.json` binds each run to its command, exit
code, logs and checkout. The adjacent build, focused, CLI, regression,
native-attempt and native logs retain complete command output. The full-gate logs and the
`previous_tree` entry are historical evidence from the earlier checkout.

This increment changes `dev/house-bend.py`, `dev/bend-policy.json`,
`dev/reference-fixtures/manifest.json` and `surface/*.bend`. Sixteen earlier
Stage A records hash some of these files: foundation, representation and
evaluation; checking, pinfront, elaboration, program and erasure, each with its
mutation record; and core-erasure, family-layout and family-erasure. These
records remain historical evidence of the previous commit and are not rewritten
with new hashes.

This increment adds family declarations and matching. Recursive definitions,
structural recursor sugar, parameterized-constructor inference, family default
arms, complete erased-corpus integration and WebAssembly build/run remain pending.
It does not complete Stage A acceptance or R2 qualification.
