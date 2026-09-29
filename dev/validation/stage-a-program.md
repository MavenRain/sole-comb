# Stage A.5b.2: recursive programs and check/print observations

This increment ports Kanon `69f3be5198cda4334de4fd23b4ccc92cac595789`
lib/order.ml, lib/totality.ml, and surface/elab.ml:994-1179 into the test-only
frontend. It completes whole-program recursive admission while preserving
the pin's first-fit structural argument search and declaration order.

## Admission and representation

Every recursive member type is elaborated against the original globals.
The types are checked as provisional postulates, then all bodies are
elaborated in the provisional environment. One totality guard covers the
whole group before translation and final checking. A group with no calls
into itself follows ordinary declaration checking. Guarded entries retain
the recursive argument index; helpers without group calls have no index.
Only the completed group enters the returned global environment.

The order certificate records the group, chosen argument, member rows, and
each call's constructor path. Its work list preserves the pinned traversal
order, including shapes, point addresses, motives, and branches. Translation
checks the head elimination, scrutinee index, motive, constructor keys, and
call chains. The pin reconstructs the same term after these checks; this
port returns the original immutable term. No reduction rule changes.

The three new unsafe declarations are `order.peel`, `order.spine`, and
`order.walk`. Their decreasing subterms are documented at the declarations
and in the policy registry. Type, affine-use, exhaustive-match, purity, and
production-import checks remain enabled. No shape dispatch is added outside
the existing shape APIs in the new frontend modules.

Totality spends one unit per term node over all bodies in a group before
certification. Direct tests cover empty, exhausted, and exact budgets.
The surface driver supplies 16384 totality units per group and the existing
16384 inference units per kernel query. Elaborator request recursion itself
remains unbounded, as in A.5b.1. These finite budgets are port backstops;
the pinned reference uses its unlimited default.

## Comparison boundary

There are 245 inputs: all 146 pinned corpus files, the 75 existing
elaboration probes, and 24 program probes. There are 89 independent expected
verdicts, with additional exact output and error expectations. The runner
compares check and print separately, for 490 observations per host.
Each comparison includes stdout bytes, stderr bytes, and the exit code.
Sixteen direct contracts cover budgets, the first and second recursive
argument positions, constructor paths, helpers, and translation refusals.

The reference recompiles every used pinned OCaml source and interface in
`_build/program/oracle/`. It verifies bytes against the pinned Git blobs
before compiling, records their hashes and the OCaml compiler version, and
checks the upstream state again afterward. The adapter follows the check
paths of bin/kanon.ml:39-59, whose hash is recorded too. The stale prebuilt
kanon executable is not used by this scoped comparison.

The Bend test driver computes one pure checked result and formats both
mode observations from it. This shares the expensive kernel work without
sharing or omitting any oracle observation. The driver transports each
modeled exit/stdout/stderr triple.
It is not the production CLI, whose endpoint remains unset. Transport
failures, unexpected host stderr, malformed framing, missing observations,
changed case identities, or changed input hashes fail the harness. The
six harness tests exercise these rejection boundaries. The replay mode
requires the recorded input hashes and the exact corpus and mode list.
Fresh oracle observations must equal the saved reference before host tests
run; the gate does not overwrite the saved reference.

## Large agreement inputs

Whole-program admission reaches the assertions after recursive groups in
the large agreement fixtures. The previous declaration-only adapter stopped
at the explicit recursive-group refusal. The first program run exceeded
300 seconds on agreement-natAdd. All corpus inputs remain required. Inputs
larger than 64 KiB have an 1800-second process backstop; smaller inputs retain
300 seconds. These test-process limits are not R2 performance evidence.

Profiling a bounded prefix identified repeated string comparisons during
global lookup. Suspending list tails and unequal string suffixes was
insufficient. The map now uses a persistent character trie with the same
public get, insert, remove, bindings, and size operations. Terminal values
precede their extensions, and character branches traverse in codepoint
order. This preserves lexical output and replacement behavior. Constructor
lookup uses ordered bindings instead of matching the map representation.
Removal drops terminal values; empty character routing nodes may remain.
Bindings and size count terminal values only.

The isolated trie implementation matched both complete agreement-natAdd
reference observations in 127.119 seconds. Its 160-line prefix took 6.106
seconds; the list implementation with short-circuiting comparison took
31.348 seconds for that prefix and exceeded 300 seconds on the full fixture.
These diagnostic runs shared the machine and are not a controlled benchmark.

The foundation suite adds 56 expectations derived from Python dictionaries
and codepoint sorting, bringing its total to 275 cases. Ascending and
descending insertion histories cover empty keys, shared prefixes,
replacement, present and absent lookups, deletion of all bindings, and
Unicode ordering across the BMP boundary. The trie adds no unsafe sites;
its recursive operations decrease the map argument before changing the key.
The foundation driver emits groups of 32 cases to respect the pinned native
backend's 255-argument limit. All 275 case results remain required.

## Deep recursive evaluation

The required `mu-cata-depth.kan` fixture evaluates a recursive natural number
with 4,095 successor constructors. It exposed nested calls in the shared request
interpreter that exhausted the JavaScript host stack. Pending requests now
keep their continuations in an explicit stack. Both completed answers and
errors pass through the original resume functions in the same order.

The public runner interface and semantic budget paths are preserved. The
existing unsafe runner entry moves to `Run.drive` with its review scope
preserved. The replacement passed both complete
reference observations for the depth fixture, then all 240 inputs below
64 KiB and the 16 direct contracts on Bun before the full regression run.

## Validation

Run `make gates` for the complete landed regression suite, including
`make test-program-mutations`. `make test-program` runs the harness tests,
fresh oracle, Bun, Node worker, native, and direct contracts. Native remains
informational for endpoint selection, but a native failure fails this target.

The three isolated mutations admit a nondecreasing argument, reverse
declaration output order, and omit the translation motive check. Each must
compile and then fail its selected observation or direct contract. Temporary
copies protect the baseline sources; source hashes are checked before and
after each complete validation run.

The bounded review traced the new admission and certificate paths against
the pinned source, checked every map comparison branch, and checked the
reference, transport, and mutation failure paths. It found that paired
baseline observations alone did not establish the single-mode transport used
by two mutants. Each selected single-mode observation is now required to
match the baseline before that mutation runs. Mutation checks also require
the hash of the fresh reference that the complete baseline compared, and
read their expected observations from that fresh copy.

`make gates` passed on 2026-09-29. The fresh pinned oracle covered all 245
inputs in both modes. Bun, Node worker, and native each matched all 490
observations and passed all 16 direct contracts. All three program mutations
were caught. The earlier regression suites, HOUSE, pin checks, and CLI
typecheck also passed. Foundation coverage now includes 275 cases per host.

The program result, reference, mutation record, foundation result, and complete
gate stdout and stderr are retained beside this document. The staged source
audit verified all 200 distinct source blobs that the program, mutation,
foundation, and re-recorded A.5b.1 records name against their recorded
hashes. The program record alone binds 193 of them.

## Review fixes, 2026-09-29

- The mutation runner now reads and hashes the fresh reference under
  `_build/program/`. The complete baseline compared that copy with the saved
  reference and recorded its hash. Before this fix, the runner hashed the
  saved reference. The saved reference records the harness hashes, so any
  harness edit stopped the mutation gate until the saved copy was replaced.
- The source audit sentence now names its scope. The number 200 counts the
  distinct blobs of all four records, not the 193 of the program record.
- The A.5b.1 record now states that this increment re-recorded its result
  and mutation records with 42 source hashes.
- After these fixes, `make gates` passed again in an isolated copy of the
  staged tree that keeps `.git`. The retained gate stdout and stderr come
  from that run. In the program and mutation records, only the hash of
  `dev/test-program-mutations.py` changed. The fresh reference is identical
  to the saved reference.

## Remaining integration

The pinned CLI supports `check --erased`; this required mode is retained for
A.5b.3 alongside its missing pinned erasure dependency. Full KANON-DIFF,
the production CLI, and Stage A acceptance remain pending. The divergence
policy and all 146 required corpus inputs are unchanged. R2 still gates the
operational compiler before M0 closure under the entry decision.
