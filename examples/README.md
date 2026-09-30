# Native sole-comb examples

Run these files through the public compiler from the repository root:

```sh
./sole-comb check examples/identity.sole-comb
./sole-comb check examples/arithmetic.sole-comb --print
./sole-comb check examples/finite-elim.sole-comb
./sole-comb check examples/motive.sole-comb
./sole-comb check examples/function-arms.sole-comb
```

`identity` demonstrates dependent functions and application. `arithmetic`
checks Nat primitives. `finite-elim` uses native braces and semicolon-separated
arms to eliminate a two-leg sum. `motive` provides an explicit return type.
Each file is self-contained and declares its Nat postulate where needed.

An `elim` arm is a function term. Named functions, partial applications, local
function values, and lambdas with multiple binders are supported. The first
parameter receives the payload; remaining parameters form the result function.
`function-arms` covers these forms, explicit motives, and linear functions.
Arms occur in the order of the sum's legs; the compiler checks completeness,
the parameter's payload type, the result type, and quantity use. A trailing
semicolon is allowed.
Legacy `case`, `match`, `rec`, branch-binder syntax, `.1`, and `.2` are reserved
and produce `E-R4-MATCH`. `with` and `absurd` remain ordinary identifiers.

Run `make test-cli` for the public command suite on Bun and Node, or:

```sh
python3 -P dev/test-cli.py --hosts bun,node-worker,native
```

`EXPECTATIONS.json` records the declaration counts and refusal expectations.
The suite also invokes every file under `corpus/refuse/`, tests lexical
boundaries, and checks command errors and file handling.

The public command currently provides source checking. Wasm build/run,
recursive families, records, constructor-labelled and default arms remain
pending. `check --print` displays checked kernel definitions rather than a
source round trip. Files larger than 32768 bytes report `E-SOURCE-SIZE`.
