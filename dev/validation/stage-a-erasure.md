# Stage A.5b.3.1 erasure support

This increment supplies the erased representation and runtime scope helpers
needed by pinned erasure. It does not yet erase checked kernel terms or add
`check --erased` to the whole-program differential. Those tasks remain in
A.5b.3.2. Full KANON-DIFF, A.close, and R2 qualification remain pending.

## Source and representation

The reference is Kanon `69f3be5198cda4334de4fd23b4ccc92cac595789`:
`lib/eterm.ml` and `lib/erase.ml:497-555`. The harness copies the complete
OCaml dependency closure, including interfaces, verifies each file against
that revision, and compiles it outside the Kanon checkout. It checks the
clean checkout and revision before and after the oracle run.

`erase/eterm.bend` preserves all five representation constructors, fourteen
term constructors, branch metadata, and both declaration constructors.
Type and function identifiers have distinct wrappers. Host indices, tags,
and arities use the existing signed Int63 representation. The branch record
is parametric in its body type to express the recursive term family in Bend.
The exact printer includes delay and force, unquoted symbolic names, large
signed literals, and OCaml-compatible escaping of UTF-8 bytes. Its escaping
helpers reuse the existing kernel printer's byte conversion, without an
import or dependency on kernel syntax printing.

`erase/runtime.bend` preserves binder depths, traversal order and duplicate
variable observations. Capture pruning sorts and deduplicates live capture
indices, preserves declaration order, and compresses the surviving indices.
It follows the pinned behavior for outer indices and empty capture lists.
Both work lists traverse finite erased syntax. The mapping work list then
rebuilds nodes from a value stack. Reindexing takes an explicit data
environment and a closed index function, as required by Bend templates.
Mapping and capture pruning return `Done` or the explicit `InvalidStack`
invariant error. Every valid comparison requires a successful result.

The modules contain 158 and 259 lines. HOUSE assigns 170 lines to the
representation and 300 to runtime helpers. The latter consumes 300 of the
planned 1,500-line erasure allocation, leaving 1,200 for the type-directed
pass. The three reviewed termination annotations cover the print, mapping,
and variable work-list loops. Type, affine-use, and exhaustiveness checks
remain enabled. No catch-all row is added.

## Validation

`make test-erasure` runs 221 cases against fresh pinned OCaml results and
on Bun, Node worker, and native. All 221 pass on each endpoint. Native is
informational for runtime selection, but a failed native check fails this
target. There are 119 independent expectations in addition to differential
comparisons. Case count, unique names, oracle row order, and independent
expectations are checked before compiling the Bend harness. The fresh
oracle output must also equal the saved observations file byte for byte.

Cases exercise all constructors, empty containers, nested let and branch
binders, 128-level term chains, repeated reads, nested closure captures,
zero and sparse capture sets, nonzero depths, negative shifts, host-index
wraparound, negative and large literals, and control/Unicode string escaping.

`make test-erasure-mutations` validates source and harness hashes, compiles
passing controls in isolated copies, and requires these changed copies to
fail their selected cases:

| Mutation | Rejected by |
|---|---|
| Ignore branch arity when entering its body | `shift/case`, `reindex/case` |
| Select capture types from the wrong end | `prune/first`, `prune/gaps` |
| Skip nested closure children | `vars/closure`, `prune/nested-closure` |

`make house test-erasure-mutations` passes. HOUSE reports 31 source files,
20 line budgets, 24 reviewed termination sites, and 11 existing catch-all
sites; its 15 policy tests pass. `check`, `test`, and `gates` now include this
support suite. The existing gate prerequisites are retained.

The full `make gates` run passed on an isolated copy of the staged tree
that keeps `.git`. It includes the preceding Stage A tests and mutation
controls, the new erased-term cases, pin checks, and the final Bend build
check. Its complete streams accompany this increment.

## Evidence files

- `stage-a-erasure.json`: endpoint counts, case names, source hashes, and
  pinned oracle provenance.
- `stage-a-erasure-observations.tsv`: the exact reference observations.
- `stage-a-erasure-mutations.json`: control and rejection evidence hashes.
- `stage-a-erasure-gates.stdout` and `.stderr`: the complete integration run.

The source and harness hashes bind these records to the tested inputs.
The saved observations file is also a gate input: `make test-erasure`
fails when the fresh oracle output differs from it. The other records are
evidence artifacts; each target performs its checks again rather than
treating the checked-in result as a passing run.

The `tid/0` and `fid/0` observations are empty identifier strings. Their TSV
rows intentionally end with the field delimiter, preserving the exact
reference bytes and recorded hash. Git's whitespace check reports these
two trailing tabs; all other staged lines pass the whitespace check.

## Bounded review

The implementation review checked the fourteen constructor paths, child
order during stack reconstruction, let and branch depth increments,
capture declaration order, signed arithmetic, source-policy additions,
and preservation of every earlier gate prerequisite. Empty-container and
deep-term cases exercise the work-stack invariants. The three mutations
provide failure evidence for the key binding and capture decisions.

## Review fixes, 2026-09-29

- The harness now compares the fresh oracle output with
  `stage-a-erasure-observations.tsv` byte for byte and names the first
  changed case. Before this fix, no target read the saved observations.
- The gate evidence now comes from one complete `make gates` pass on an
  isolated copy of the staged tree that keeps `.git`, with the logs in this
  repository. The earlier capture came from an external checkout, and its
  command record named that checkout. That record is removed.

Current provenance: recorded Kanon fixtures; see no-ocaml-cleanup.md.
