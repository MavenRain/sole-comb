# Stage A.5a: pinned lexer and parser

This slice ports the pinned test frontend's tokens, byte lexer, surface AST,
recursive descent parser, and canonical syntax printer to Bend under
`test/pinfront/`. It builds on A.4 (`ff96765`). Elaboration, kernel admission
from source, and the full KANON-DIFF modes remain A.5b work.

The source reference is Kanon
`69f3be5198cda4334de4fd23b4ccc92cac595789`. `test/kanon/` contains all 146
`.kan` files from that revision's `test/` and `examples/` trees, without
filtering or edits. `SOURCE.json` and `SURVIVORS.tsv` record their provenance
and byte hashes. Every input is included in Stage A; R4 survivor decisions
remain pending. The gate checks the manifest against the copied files and
the pinned Git blobs, including missing, extra, duplicate, and reordered
members.

The 561 comparison cases comprise the complete corpus and 415 supplemental
cases. These cover all 256 single bytes, escaped and UTF-8 byte literals,
byte positions, every syntax family, binder rollback, constructor lists,
recursive groups, projections, bounded numeric fields, and 40,000-character
comments and identifiers. Sixty-eight cases
also have fixed expectations independent of the OCaml adapter.

For each input, the gate compares the complete token stream with byte
locations, structural AST or exact refusal, canonical printed source, and
print/parse round-trip result. A length-framed observation preserves embedded
newlines and arbitrary bytes. The Python runner rejects malformed, missing,
reordered, duplicate, or extra output before comparing every observation
byte. Expected observations are kept out of the generated Bend program so
large fixtures do not overflow the compiler while reading string literals.

`test/pinfront-oracle.ml` only serializes observations from the unchanged
pinned OCaml lexer, parser, and printer. The gate compiles their minimal
dependency closure afresh outside the Kanon checkout, verifies each source
against its pinned blob, and checks the checkout is clean before and after.
This is reference-test glue; the port itself is Bend. The saved reference
records the compiler, dependency versions, copied source hashes, generated
adapter, and executable hash. `--replay` trusts the saved observations. It
checks only the configured pin revision string, the input hashes, the case
identities, and the source hashes. Each oracle run compares the name, source
hash, and observation of every fresh case with the saved reference, in
order. A difference stops the gate until a reviewer copies the fresh
reference.

The Bend parser uses a typed task dispatcher for mutually recursive grammar
productions. Its explicit results retain the pinned error messages and
locations. Syntax and observation rendering also use typed tasks. UTF-8
conversion, token-list rendering, and long observation comparison use
accumulators so corpus size does not consume the JavaScript call stack.
The span scanner also accumulates characters, covering long comments and
identifiers that previously reproduced a stack overflow during review.
The registered `@unsafe` sites cover recursion through task states or
derived token suffixes, with reasons in `dev/bend-policy.json`.

HOUSE applies the pure-source policy and reviewed recursion/catchall
registry to `test/pinfront/`. Each catchall entry records the exact number
of catch-all rows in its function. A separate check starts from the
production entry and every module under `bin`, `lib`, `surface`, `erase`,
and `wasm`. It follows their imports, resolves aliases, parent components,
symlinks, and case variants, and rejects any route into the test frontend. Test modules do not count toward
the trusted production library's line budgets.

Native uses the same pinned Bend C emitter and a small runtime-input entry
in `test/pinfront-native.bend`, outside the pure frontend. The C dispatcher
is compiled with the pinned driver's clang `-O3` CPU flags for this behavioral
INFO check. The initial build with the corpus embedded exceeded the
600-second command deadline, motivating the runtime-input entry. Compiler
identity, flags, emitted C, and executable hashes are recorded. This test
does not qualify native performance or change the earlier native gates.

`make gates` passed on 2026-09-28 with these results:

| Check | Result |
| --- | --- |
| A.1, A.2, A.3, A.4 regressions | 219, 465, 251, and 291 cases respectively on Bun, Node worker, and native INFO |
| A.4 behavioral mutations | All three rejected |
| A.5a parser comparisons | All 561 cases agree on Bun, Node worker, and native INFO; 68 independent expectations pass |
| A.5a behavioral mutations | All five killed by observation mismatches |
| Harness failure injection | Nine tests pass |
| HOUSE | Fifteen tests pass; 24 files, 18 line budgets, 15 reviewed unsafe sites, five catchalls |
| Pins and host build check | All 35 pin checks and the final host check pass |
| Earlier build, benchmark, and R2 harness regressions | All pass |

Compilation or malformed output never counts as killing a mutation. The
[result record](stage-a-pinfront.json) and
[mutation record](stage-a-pinfront-mutations.json) bind the checks to source
hashes. The complete [reference observations](stage-a-pinfront-reference.json)
include the pinned OCaml build provenance. Full gate output is retained in
[stdout](stage-a-pinfront-gates.stdout.log) and
[stderr](stage-a-pinfront-gates.stderr.log). The oracle run compared its
fresh cases with the saved reference. The `reference_sha256` field of the
result record binds the saved copy by hash.

The five mutations change match flavor, numeric bounds, byte-column
advancement, retained family constructors, and UTF-8 encoding. They run on
copies and verify that the original source hashes remain unchanged.

The bounded review covered corpus and reference provenance, malformed-output
handling, replay identity checks, parser backtracking and bounded numeric
fields, retained family constructors, the production import closure, and
Makefile gate preservation. It found the long-span stack overflow fixed
above. The import checker was compared with the pinned Bend loader, which
requires local module paths to end in `.bend`. No existing gate was removed
or narrowed. Validation ran in an isolated checkout with the same source
bytes staged for the real repository.

Run `make test-pinfront`, `make test-pinfront-mutations`, and `make gates`
with the locally pinned tools. This evidence is scoped to A.5a. Full
`check`, `check --print`, and `check --erased` comparisons, divergence-policy
closure, A.close mutants, BUILD-TIME, R2 qualification, and Stage A acceptance
remain pending.

## Review fixes, 2026-09-28

- The production import check now starts from every module under `bin`,
  `lib`, `surface`, `erase`, and `wasm`. Before this fix, it started only
  from the entry stub, so it did not check the library modules.
- Each catchall registry entry now records its exact number of catch-all
  rows. Before this fix, one entry covered all 45 rows of the parser
  dispatcher, so a new row needed no registry change.
- The import check now also rejects a case variant of `test/pinfront`, such
  as `TEST/pinfront`, on a case-insensitive file system.
- Each oracle run now compares every fresh case with the saved reference.
  Before this fix, the gate checked only the probe and expectation counts,
  so a changed probe input did not stop it.
- This record now states what `--replay` checks and what it trusts.
- This record no longer claims a replay of the saved reference. The gate
  did not run one.
- The gate rerun used an isolated copy of the staged tree that keeps `.git`.
  The result, reference, and mutation records and the gate output come from
  that rerun.

Current provenance: recorded Kanon fixtures; see no-ocaml-cleanup.md.
