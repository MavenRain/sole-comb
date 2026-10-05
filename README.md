# sole-comb

A language with Kan extensions as its type-forming primitives and one primitive
sum eliminator. The compiler host is Bend 2; the intended target is WebAssembly.

UAT.0 preparation now pins the selected Lean proof baseline and its dependencies,
records theorem types and transitive axioms, and checks the current public-source
prerequisites on all three hosts. See the [readiness evidence](dev/validation/uat-readiness.md)
and [UAT implementation schedule](dev/uat-proof-portability.md).

The public compiler now checks `.sole-comb` source files through production
modules under `surface/` and the real kernel. Start with:

```sh
./sole-comb check examples/identity.sole-comb
./sole-comb check examples/finite-elim.sole-comb
./sole-comb check --erased examples/erasure.sole-comb
./sole-comb check --erased examples/product-erasure.sole-comb
./sole-comb check --erased examples/sum-erasure.sole-comb
./sole-comb check --erased examples/pair-erasure.sole-comb
make test-cli
```

The [native examples](examples/README.md) cover dependent functions, Nat
primitives, closed records with qualified getters, and finite-sum `elim` with
optional motives and final `else: function` defaults. Records expand to Ran
products and Out projections; each default expands into checked positional arms. The public command
builds its host compiler on demand and checks the file's exact bytes.
`make check`, `make test`, and `make gates` include these public-command tests.
The [public compiler decision](dev/public-compiler-entry.md) moves this work
ahead of the former Stage C frontend timing.

The real Stage A kernel port has started under `lib/`. A.1 provides the
foundation, finite budgets, typed errors, universe levels, and literals.
A.2 adds terms, semantic values, shapes, globals, and the pinned Nat primitives.
A.3 adds evaluation, readback, and typed conversion with focused oracle comparisons.
A.4 adds bidirectional checking, declaration and family admission, positivity,
quantity accounting, printing, and specification counts.
A.5a adds the test-only pinned lexer/parser port, canonical syntax printing,
and parser comparisons across the complete 146-file Kanon corpus.
A.5b.1 adds expression elaboration, ordinary declarations, and mutual family
admission, with exact declaration observations across the same corpus and
75 focused probes. See the [elaboration evidence](dev/validation/stage-a-elaboration.md).
A.5b.2 adds recursive groups, structural certificates, totality budgets, and
whole-program check/print comparisons. See the [program evidence](dev/validation/stage-a-program.md).
A.5b.3.1 adds the erased representation, exact printer, and runtime scope
and capture operations. See the [erasure support evidence](dev/validation/stage-a-erasure.md).
The build plan is `../kan-elim-lang-m0/M0-PLAN.md`, with the approved
[Stage A entry decision](dev/stage-a-entry.md). A.5b.3.2a adds type-directed
erasure for Nat values and ordinary functions through `check --erased`, including
ghost arguments, eta expansion and closure capture pruning. See the
[core erasure evidence](dev/validation/stage-a-core-erasure.md). A.5b.3.2b adds
collection tuples, closed-record layouts and field projections, including erased
fields, nested products and function fields. See the
[product erasure evidence](dev/validation/stage-a-product-erasure.md).
A.5b.3.2c adds finite-sum layouts, injections and case erasure, including erased
payloads, default arms and captured branch closures. See the
[sum erasure evidence](dev/validation/stage-a-sum-erasure.md).
A.5b.3.2d adds dependent-pair layouts, introductions, and native `elim` with one
two-binder lambda arm. Erasure drops type and proof fields, evaluates the
scrutinee once, and preserves runtime indices through nested pairs and closures.
See the [pair erasure evidence](dev/validation/stage-a-pair-erasure.md).
The A.5b.3.2e.1 increment adds nominal family and constructor layout planning.
It opens family parameters as variables, drops erased fields, and records
constructor layouts in declaration order. See the
[family layout evidence](dev/validation/stage-a-family-layout.md).
A.5b.3.2e.2 adds recursive-family constructor and match erasure through the
kernel test frontend. It preserves nominal layouts, generic erased placeholders,
dependent branch targets, runtime binder indices and complete constructor groups.
See the [family erasure evidence](dev/validation/stage-a-family-erasure.md).
Its first native checks timed out at the tracked 900-second limit and passed with a local 2400-second deadline. The review rerun passed with the tracked limits.
A.5b.3.2e.3 adds public brace-form `mu` declarations and positional `elim`
arms for recursive families. It supports mutual groups with `and`, parameters,
indexed motives, empty families, and typed constructor fields. Constructors are
separated by semicolons, and a trailing semicolon is allowed. Family elimination
requires an explicit motive. See the
[public family evidence](dev/validation/stage-a-public-family.md).
A.5b.3.2e.4 adds constructor parameter inference from an expected family type.
It checks dependent fields in their instantiated telescope. It forwards expected
types through constructor applications and nullary constructor names. This also
applies to a family without parameters: each constructor argument gets its field
type as the expected type. Examples cover open parameters, aliases, nested
constructors, argument positions, indexed families, a wrapper without parameters
and a list. See
[constructor parameter validation](dev/validation/stage-a-constructor-parameters.md).

A.5b.3.2e.5 forwards expected types to family parameters and indices. It opens
the combined telescope in declaration order, so later arguments can depend on
earlier ones. In the family argument `Hold (box 7)` of
[`examples/family-arguments.sole-comb`](examples/family-arguments.sole-comb),
the constructor `box 7` now gets its expected type `Box Nat`. The checker
accepts nested and nullary constructors, aliases and open variables in family
arguments. See [family argument validation](dev/validation/stage-a-family-arguments.md).
A.5b.3.2e.6 adds constructor parameter inference from field types when the
constructor has no expected family. Direct parameter occurrences and nominal
family parameters and indices provide constraints. Later fields receive their
instantiated expected types. The result carries a kernel-checked family
annotation, so a constructor can be an unannotated elimination scrutinee.
The [constructor inference example](examples/constructor-inference.sole-comb)
covers nested constructors, distinct and open parameters, recursive tails and
value parameters inferred from a nominal index. See
[constructor inference validation](dev/validation/stage-a-constructor-inference.md).

Constructors of families without parameters also become unannotated scrutinees.
For `mu Token : Type 0 { token : Token }`, `check --print` shows
`def value : Nat := elim (token) as x in Token return Nat { 7 }` as follows:

```text
def value : Nat := (Elim SMu Token [] ((In SMu Token [] (ACtor token) []) : (Lan SMu Token [] (Sec SColl 0 []))) as x return Nat with | (ACtor token)  => 7)
```

Inference reads the fields from left to right. It first puts the solved
parameters and the earlier field values into the field type. If no unsolved
parameter remains in that type, the checker checks the argument against it. If
an unsolved parameter remains in any position, the checker does not use the
field type as an expected type. It infers the type of the argument and solves
the parameters from that type. A parameter gets a value at the top of the field
type and in the parameters and indices of a nominal family. A parameter beneath
a function, product or sum former gets no value from that field. Another field
must supply it. The kernel then checks the annotated constructor with all its
fields. An argument without a type of its own, such as a lambda or a tuple
without an annotation, cannot be inferred. In a field with an unsolved
parameter, the checker refuses it with a text that names the constructor and the
field. A nullary constructor of a family with parameters, such as `nil`, is
refused in the same way. For `r : (Nat -> A) -> A -> R A`,
`r (fun (n : Nat) => n) 7` is refused. For `r2 : A -> (Nat -> A) -> R2 A`,
`r2 7 (fun (n : Nat) => n)` is accepted. For `w3 : Both (Nat -> A) A -> W3 A`
and `g4 : Nat -> Nat`, `w3 (both g4 7)` is accepted. The second parameter of
`Both` gives the value of A. Nullary and phantom parameters without field
evidence remain refused.

A.5b.3.2e.7 adds a final `else: term` arm for public families. It covers
the remaining constructors in declaration order and checks each copy with
that constructor's fields, quantities and motive result. One default value
must fit every remaining constructor. A copy that does not fit gets the same
error as the equivalent explicit arm. See the
[family default examples](examples/family-default-arms.sole-comb) and
[validation](dev/validation/stage-a-family-defaults.md).

A.5b.3.2e.9 adds [direct recursive-field layouts](dev/validation/stage-a-recursor-layout.md)
for the structural recursor. The planner retains field quantities, normalized
domains and child indices in declaration order. It refuses a field that
mentions another member of a mutual group, as it refuses a non-direct self
occurrence. Public recursor typing, delayed induction hypotheses and their
erasure remain pending.

A.5b.3.2e.10 adds [uniform-parameter validation](dev/validation/stage-a-recursor-uniform-layout.md)
for those layouts. Typed conversion checks each recursive child's parameters
against the declaration parameters, including dependent parameters and erased
children. Transparent aliases and proof-irrelevant parameters are accepted;
changed parameters receive an explicit refusal. Public structural recursor
typing, evaluation and erasure remain pending.

A.5b.3.2e.11 adds [checked recursor motives and hypothesis types](dev/validation/stage-a-recursor-motive.md).
It checks the motive in the declaration parameter context, preserves indexed
and self-dependent result types, and applies the existing universe restriction
on elimination from propositions. Public branch binders, delayed recursive
evaluation and recursor erasure remain pending.

A.5b.3.2e.12 adds [checked recursor branch types](dev/validation/stage-a-recursor-branch.md).
It binds the constructor fields, then one induction hypothesis for each
direct recursive child. The kernel checks every binder domain and the
elimination target. Affine recursive children receive an explicit refusal.
Branch bodies, public elimination hypotheses, delayed recursive evaluation
and recursor erasure remain pending.

Recursive definitions, structural recursor sugar, complete erased-corpus
integration, WebAssembly build/run and the full kernel differential remain
pending.
The constructor inference validation records the current host checks and
integration gate status.

Run from this directory with the locally pinned tools:

```sh
make build           # cached JavaScript build of the host entry
make check           # host entry, library tests, and scoped source policies
make test            # build, existing regressions, landed Stage A checks, policies, pins
make gates           # existing regressions, landed Stage A checks, policies, pins
make test-foundation # real A.1 library on Bun, Node worker, and native (INFO)
make test-representation # real A.2 dependency closure on the same hosts
make test-evaluation # real A.3 evaluator and converter on the same hosts
make test-checking   # A.4 checker against pinned reference fixtures on all three hosts
make test-checking-mutations # checker comparisons and three isolated mutations
make test-pinfront   # pinned lexer/parser comparisons on all three hosts
make test-pinfront-mutations # parser comparisons and five isolated mutations
make test-elaboration # expression/declaration comparisons on all three hosts
make test-elaboration-mutations # elaboration comparisons and three isolated mutations
make test-program     # whole-program check/print comparisons and direct contracts
make test-program-mutations # program comparisons and three isolated mutations
make test-erasure     # erased representation and runtime scope against pinned Kanon
make test-erasure-mutations # erasure support comparisons and three isolated mutations
make test-core-erasure # ordinary and product erasure against pinned reference fixtures
make test-core-erasure-mutations # quantity, binding, layout and projection mutations
make test-family-layout # nominal family layouts against pinned reference fixtures
make test-family-layout-mutations # layout comparisons and four semantic mutations
make test-family-erasure # family constructor and match erasure against pinned reference fixtures
make test-family-erasure-mutations # family erasure comparisons and eight semantic mutations
make test-public-family # public families against frozen Kanon output
make test-public-family-mutations # public families and four sugar mutations
make test-family-defaults # family defaults, explicit expansions and refusals on all hosts
make test-family-defaults-mutations # family defaults and four semantic mutations
make house           # scoped source policies and landed module line budgets
make bench-preflight # pins and current load; no benchmark samples
```

`dev/toolchain.json` retains `endpoint: null` until S0-5 selects Bun or the Node
worker using valid measurements. `./sole-comb` is a tracked public source
driver. It uses Bun for development while the endpoint is unset, or the selected
endpoint afterward. `check --host bun|node-worker|native` overrides that host
for validation. A development default does not select the qualified endpoint.
No command installs tools or updates the pinned Bend checkout.

Semantic regression tests use immutable Kanon observations under
`dev/reference-fixtures/`, including descriptors for the existing parser,
elaboration, and program reference snapshots. The loader checks the fixture and
referenced snapshot checksums, the Kanon
revision, and the exact ordered case inputs before comparing the Bend results.
The existing independent expectations and semantic mutation tests remain active.
`make test-reference-fixtures` checks fixture integrity and rejects OCaml source
files anywhere in the repository outside build and Git directories. The
[no-OCaml cleanup evidence](dev/validation/no-ocaml-cleanup.md) records the
provenance of each fixture.

These observations were captured from verified pinned sources before removing
the adapters at sole-comb revision `2a22f67b7f2e18a435d1dfbb969ba78a1a6300e5`.
Routine semantic tests use recorded results and do not compile a live oracle.
Historical validation documents describe the earlier live comparisons. To
refresh expectations, run the pinned reference tooling outside sole-comb,
review the observations and independent expectations, and update the affected
fixture and its manifest checksum. Never derive reference expectations from
sole-comb's own output. Changed inputs or revisions require a reviewed refresh.

The active work is the [Stage A compiler milestones](dev/stage-a-milestones.md).
The [A.1 validation](dev/validation/stage-a-foundation.md),
[A.2 validation](dev/validation/stage-a-representation.md),
[A.3 validation](dev/validation/stage-a-evaluation.md),
[A.4 validation](dev/validation/stage-a-checking.md),
[A.5a validation](dev/validation/stage-a-pinfront.md),
[A.5b.1 validation](dev/validation/stage-a-elaboration.md),
[A.5b.2 validation](dev/validation/stage-a-program.md), and
[A.5b.3.1 validation](dev/validation/stage-a-erasure.md) record their scope.
The CLI now reaches production parsing, elaboration, and kernel checking.
`build` and `run`, recursive definitions, and dependent or parameterized records require
later compiler work and fail explicitly. Complete erasure and
KANON-DIFF remain A.5b.3.2 work. Stage A acceptance remains pending.

R2 is pending and will qualify the operational compiler before M0 closure.
The [workload manifest](dev/r2-workloads.json) and scratch validation records
retain their evidence, but further scratch compiler features are retired.
Complete source qualification and any language-alignment changes to the
twins are required before timing. No runtime endpoint has been selected.

## Benchmark measurements

`dev/bench.sh NAME 'exec COMMAND ARGS'` preserves the original interface. It runs
one warm-up and five timed samples by default. `RUNS`, `BENCH_POLL_S` and
`SOLE_COMB_TOOLCHAIN` remain supported. Use the Python entry for a JSON report:

```sh
python3 -P dev/bench.py probe --argv '/absolute/path/to/command input' \
  --require-threads --cache-state cold --json _build/bench/probe.json
```

`--argv` splits the string with shell quoting rules and runs it without a
shell, so the child CPU is the rusage of the measured program. S0-5 legs use
`--argv` with no `exec`. Without `--argv`, and in `dev/bench.sh`, the command
runs under `/bin/zsh -f -c`; that CPU also includes the zsh start-up (about
3 ms on darwin). The JSON `launcher` field records which path ran.

Use `--accept-line 'All terms check.'` for Bend's check-only denominator.
This requires exit zero and that complete stdout line. Do not label a run's
compile cache `cold` or `warm` unless the caller has established that state;
the default is `unknown`. This option records a declaration, not a cache reset.
S0-5 requires five timed samples, regardless of the generic harness's `RUNS`
override.

The harness verifies the toolchain pins before running a leg. It reads the load
ceiling, total load-wait budget and per-child deadline from the pin file. An
above-ceiling load before a run waits. An observed load spike during or after a
run discards that attempt and repeats the same sample within the wait budget.
Discarded attempts remain in JSON and cannot contribute to the median. Warm-up
CPU is also excluded. Failure, timeout or interruption produces no median.

Each sample records wall time, child user CPU, child system CPU, their sum,
observed peak thread count and load before, during and after execution. CPU
comes from `wait4` for that child, excluding the observer thread's work. On
the shell path it includes the `/bin/zsh -f` start-up CPU. Wall
time includes process launch and observer setup. Thread and load observations
are sampled every 5 ms; this is not CPU affinity or an exhaustive thread count.
Darwin uses `proc_pidinfo`, Linux uses `/proc`. Missing thread observations are
explicit; `--require-threads` refuses such a leg.

Commands must remain in the foreground and reap their children. A deadline or
keyboard interruption kills the process group. Detached children are outside
the measurement contract. Output excerpts and full-stream hashes are retained
in JSON. The report also includes the toolchain file hash and pin-check result.

Exit codes are 0 for a completed measurement or ready preflight, 1 for a pin or
child failure, 2 for command-line usage, 3 for an unmet measurement condition,
and 130 for interruption. Preflight reports `READY` or `NOT_READY` without
waiting. Neither is an R2 verdict or evidence that the full load-wait budget
elapsed. `PASS` means a measurement leg completed; it is not an R2 ratio verdict.

## Historical Stage 0 evidence

The [probe preparer](dev/r2-prepare.md),
[serial measurement runner](dev/r2-run.md), and
[R2 evidence assembler](dev/r2-risk.md) are ready for S0-5.
`make test-r2-prepare` checks builds, correctness qualification, and the
hash-bound handoff. Once the scratch probe and twins exist, `make r2-prepare
R2_PREPARE_PLAN=/absolute/path/to/prepare-plan.json R2_OUTPUT=/new/scratch/directory`
builds the endpoints and publishes `run-plan.json` only after its checks pass.
`make test-r2-run` checks collection, cancellation, and confirmation;
`make test-r2` checks evidence refusal and selection rules. Once the scratch
probe and twins exist, `make r2-run R2_PLAN=/absolute/path/to/run-plan.json
R2_OUTPUT=/new/scratch/directory` collects the required legs. Each attempt
preserves raw measurements and a hash-bound manifest. A GREEN first round
automatically gets an independent confirmation.

The [scratch Bend lexer](dev/validation/s0-5-r2-lexer.md) passes 35 correctness
cases on Bun, the pinned Node worker, and the informational native build.
Its source, tests, and six detected mutations remain under the scratch path.
This is frontend development evidence; it does not qualify a probe endpoint.

The [scratch Bend parser](dev/validation/s0-5-r2-parser.md) passes 75 syntax
and refusal cases on the same three runtimes, with eight detected mutations.
It builds explicit syntax trees for the documented probe subset, preserves
arm and declaration order, and refuses pattern keywords. Its fuel-bounded
state machine has no unsafe declarations. Name resolution, sugar expansion,
typing, and conversion belong to the scratch checker.

The [scratch name resolver](dev/validation/s0-5-r2-resolver.md) passes 76
exact-tree and refusal cases on those three runtimes, with ten detected
mutations. It resolves local names to binder indices and checks ordered,
nonrecursive global definitions. Record declarations and `else` arms
explicitly require sugar expansion. This is the first checker pass.

The [scratch conversion engine](dev/validation/s0-5-r2-conversion.md) passes
107 exact-normal-form, comparison, and refusal cases on all three runtimes,
with twenty detected mutations. It implements capture-safe normalization
and alpha/beta/delta/zeta conversion for the resolved function fragment.
The conversion engine is untyped; the scratch typechecker supplies the
function-fragment typing checks.

The [scratch typechecker](dev/validation/s0-5-r2-typing.md) passes 96 exact
signature and refusal cases on all three runtimes, with nineteen detected
mutations. Its source entry point connects lexing, parsing, name resolution,
and bidirectional typing for dependent functions, checked annotations and
`let` bindings, exact universe levels, and impredicative `Prop` products.
Type-directed conversion rules (eta and proof irrelevance), record expansion,
and projection typing and reduction remain pending.

The [scratch arm expander](dev/validation/s0-5-r2-arms.md) passes 57 exact-tree
and refusal cases on all three runtimes, with twelve detected mutations.
Given constructor names in declaration order, it checks positional labels
and expands a final `else` into the remaining arms without changing bodies.
It does not infer constructor metadata or traverse nested eliminations;
connecting it to data checking and recursive sugar processing remains pending.

The [scratch data checker](dev/validation/s0-5-r2-data.md) passes 40 exact
signature and refusal cases on all three runtimes, with nine mutations that
Bun detects. It takes non-indexed data types with parameters from caller
metadata, checks constructor spines in checking mode, and checks `elim` with
an explicit `as x return M` motive. K1 iota reduces an elimination of a
constructor with all of its fields. Indexed families, motive inference,
records, projections, and the connection to the arm expander remain pending.

The [complete workload translations](dev/validation/s0-5-r2-twins.md) map
all 19 core declarations and preserve all four obligations. Both full
inputs pass lexing on Bun, the pinned Node worker, and native (INFO).
Their source checker qualification remains pending.

The [workload parser extension](dev/validation/s0-5-r2-workload-parser.md)
preserves the complete syntax trees of both twins and all three negative
fixtures. It adds brace-form `mu`, typed parameters and index
telescopes, indexed motives, scratch literal syntax, and zero-arity
sum/product/tuple forms. Source-derived data metadata, resolution of the
new nodes, and their typing and reduction rules remain step 2 work.

The scratch checks above are historical evidence. The
[entry decision](dev/stage-a-entry.md) retires further scratch feature work.
Complete source qualification, preparation, informational native builds,
Bun and Node worker measurements, startup measurements, and the required
GREEN repeat remain pending against the operational real compiler.
Scratch compiler code remains outside this tree. No R2 verdict or runtime
selection has been made.

`make r2-report R2_MANIFEST=/absolute/path/to/manifest.json` verifies collected
evidence and writes `dev/r2-risk.json`. Reports preserve raw samples, source
and bundle hashes, pinned and observed tool hashes, startup shares, and the
required GREEN confirmation. The runner never changes the pinned endpoint.

The plan's load ceiling is 8.0 with a 3600-second wait budget. Endpoint
selection remains pending until R2 has valid evidence. Stage A is underway
under the approved sequencing change. Saved preflight observations are historical;
rerun `make bench-preflight` before measuring.
