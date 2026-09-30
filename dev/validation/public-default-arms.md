# Public default-arm validation

On 2026-09-30, `make test-cli` passed 118 public-command observations on Bun,
Node worker, and native. Each host checked seven native examples, rejected
twenty-eight source fixtures, and exercised two lexical boundary cases.
Seven additional observations covered command errors and file handling.
The suite required identical printed definitions across hosts and verified
that all 73 recorded input hashes stayed stable. Every observation's stdout,
stderr, and exit status is retained in `public-default-arms.json`.

The native parser accepts `else: term` only as the final elimination arm,
with an optional trailing semicolon. It requires the colon and a body and
rejects duplicate or non-final defaults. `else` is a lexical keyword, so it
cannot become an ordinary arm head or declaration name. Comments and byte
literals containing the word remain ordinary lexical data.

`surface/sugar.bend` expands a default into positional arms for the remaining
legs of the checked scrutinee's collection. Every generated arm uses the
same lambda normalization as an explicit arm, preserving the payload binder,
its annotation and quantity, and any remaining function binders. Branch
elaboration receives only ordinary arms and checks each payload and result
independently. Empty sums and defaults after an already complete arm list
are refused because a default must cover at least one remaining leg.
An explicit arm past the last leg stops the expansion, so branch elaboration
reports the kernel leg-address refusal before it considers any default.
The expansion's list arguments decrease structurally under Bend's termination
checker. The increment adds no host `@unsafe` site or kernel primitive.

`examples/default-arms.sole-comb` and `examples/default-arms-explicit.sole-comb`
each contain twenty declarations. The latter independently writes every
expanded arm. All three hosts required byte-identical printed kernel
definitions between these files, after removing the command's file header.
They cover default-only and partial coverage, named and local functions,
partial applications, lambdas with multiple binders, explicit motives,
linear functions, outer local and scrutinee captures, nested eliminations,
one-leg sums, and a heterogeneous sum whose remaining payloads agree.

Thirteen new source refusals cover the colon, body, final position, duplicate
defaults, missing closing brace, empty or already covered sums, an explicit
arm past the last leg before a default, nonfunctions, wrong domains and
results, incompatible remaining payloads, and linear duplication. The
incompatible-payload fixture checks that a named default valid for the first
leg is still rejected at a later leg. The example census and the independent
expansion pair are required by the public harness.

`make house` passed all fifteen policy tests and the scoped host checks:
43 files, 32 line budgets, 35 reviewed unsafe sites, and 24 catchall functions.
The policy records the three new parser refusal boundaries and moves the
existing arm-normalization catchall into `surface/sugar.bend`.
`python3 -P dev/build.py --check` passed the complete public production
dependency closure.

The policy change moves the `dev/bend-policy.json` hash that the A.5b.1
elaboration records pin. `make test-elaboration-mutations` then ran to
completion on the final sources. All 221 A.5b.1 cases agreed on Bun, Node
worker, and native, and all three elaboration mutations were caught.
`stage-a-elaboration.json` and `stage-a-elaboration-mutations.json` come from
that run. That gate checks the pinned frontend under `test/pinfront/`, which
has no source changes here. `make test-cli` checks the public path through
`surface/elab.bend`.

An earlier broader `make gates` run, before the review fixes, passed A.1
through A.5a on all three hosts, including checking mutations and all five
parser mutations. It was stopped after the A.5b.1 oracle completed 221 cases.
Its partial logs are retained in `public-default-arms.gates.*.log`. The
program and erasure gates did not run on the final sources. Full KANON-DIFF,
Stage A acceptance, and R2 qualification remain pending.

Type-directed erasure, constructor-labelled arms, records, recursive-family
recursors, and WebAssembly build/run remain pending. The kernel and pinned
frontend under `test/pinfront/` have no source changes in this increment.

## Review fixes

Before the fixes, an explicit arm past the last leg that preceded a default
produced the misleading "a default elim arm must cover at least one remaining
leg". The expansion now stops when the legs run out, and
`corpus/refuse/default-excess.sole-comb` pins the kernel leg-address
refusal. `surface/syntax.bend` renders a default arm with the same ` | `
separator as the other arms. The two A.5b.1 elaboration records now pin the
staged `dev/bend-policy.json`.
