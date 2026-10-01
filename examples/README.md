# Native sole-comb examples

Run these files through the public compiler from the repository root:

```sh
./sole-comb check examples/identity.sole-comb
./sole-comb check examples/arithmetic.sole-comb --print
./sole-comb check examples/finite-elim.sole-comb
./sole-comb check examples/motive.sole-comb
./sole-comb check examples/function-arms.sole-comb
./sole-comb check examples/default-arms.sole-comb
./sole-comb check examples/records.sole-comb --print
./sole-comb check --erased examples/product-erasure.sole-comb
./sole-comb check --erased examples/sum-erasure.sole-comb
```

`identity` demonstrates dependent functions and application. `arithmetic`
checks Nat primitives. `finite-elim` uses native braces and semicolon-separated
arms to eliminate a two-leg sum. `motive` provides an explicit return type.
Each file is self-contained and declares its Nat postulate where needed.

`sum-erasure` preserves tag numbers while dropping type and empty-product payloads.
It also covers nested product/sum layouts, function payloads, captured branch
closures, zero-quantity binders, explicit motives and empty case dispatch.

An `elim` arm is a function term. Named functions, partial applications, local
function values, and lambdas with multiple binders are supported. The first
parameter receives the payload; remaining parameters form the result function.
`function-arms` covers these forms, explicit motives, and linear functions.
Arms occur in the order of the sum's legs; the compiler checks completeness,
the parameter's payload type, the result type, and quantity use. A trailing
semicolon is allowed.

Write `else: function` as the final arm to fill every remaining positional leg.
For example, `elim x { first; else: id }` keeps `first` at leg zero and uses
`id` for each later leg. Defaults support the same functions and lambdas as
explicit arms. Each generated arm is checked against its own payload and
result types, including motives and quantity use. A default must cover at
least one leg. A trailing semicolon is allowed.
`default-arms` covers defaults with named, local, partially applied, linear,
and multi-binder functions, nested eliminations, and scrutinee captures.
`default-arms-explicit` independently writes every expanded positional arm;
the public suite requires both files to print identical kernel definitions.
Legacy `case`, `match`, `rec`, branch-binder syntax, `.1`, and `.2` are reserved
and produce `E-R4-MATCH`. `with` and `absurd` remain ordinary identifiers.

Declare a closed record with `record Pair : Type 0 { first : Nat; second : Nat }`.
Construct a value with `tuple(10, 20)` at type `Pair`, and apply `Pair.first` or
`Pair.second` to it. The declaration expands to a Ran product alias and one
checked Out getter per field. Getters are ordinary functions and can be passed
as `elim` arms. Each getter consumes its record once, so it accepts linear
record values. Labels are scoped by the record name. An empty record uses
`{}` and `tuple()`. A trailing semicolon is allowed.

Field types may reference preceding globals, including other records. They
are independent of the record's other fields; dependent fields, recursive
fields, and record parameters remain pending. Record types use the existing
structural product equality. Use the qualified getter form `Pair.first p`.
`records` covers multiple fields, nesting, functions, universes, and name
capture. `record-expansion` and `record-expansion-explicit` must print the same
kernel definitions after renaming the generated binder; `records.kernel` records the reviewed field-index and
type expectations for the larger example.

Run `make test-cli` for the public command suite on all three hosts, or:

```sh
python3 -P dev/test-cli.py --hosts bun,node-worker,native
```

`EXPECTATIONS.json` records the declaration counts and refusal expectations.
The suite also invokes every file under `corpus/refuse/`, tests lexical
boundaries, and checks command errors and file handling.

`check --erased` prints type-directed erased code for Nat values and ordinary
functions, collection products and finite sums. `erasure` covers ghost arguments, partial applications, eta
expansion, captured and unused runtime variables, and linear parameters.
Universes, proof positions and empty products, such as its `Unit`, disappear.
`product-erasure` covers closed-record getters, erased type and proof fields,
runtime projection indices, nested products, function fields, linear parameters,
captured variables, and empty or entirely erased tuples. Dependent pairs,
recursive families and recursive definitions report an
explicit erasure refusal.

The public command provides source checking and ordinary, product and sum erasure. Wasm build/run,
recursive families, dependent or parameterized records, and constructor-labelled arms remain
pending. `check --print` displays checked kernel definitions rather than a
source round trip. Files larger than 32768 bytes report `E-SOURCE-SIZE`.
