# No-OCaml cleanup

This change removes the OCaml oracle adapters from sole-comb. Routine gates
now read recorded Kanon observations. They do not compile or run a live
oracle, and they do not need the Kanon dune build.

## Scope

The change deletes seven adapters, `test/*-oracle.ml`. It adds eight frozen
fixtures under `dev/reference-fixtures/`, one for each scope: checking,
evaluation, erasure, pinfront, elaboration, program, core-erasure and
family-layout. `dev/reference-fixtures/manifest.json` holds the sha256 of each
fixture. The loader `dev/reference-fixtures.py` refuses a changed manifest,
fixture or snapshot, a changed Kanon revision, and changed case inputs.
`dev/test-reference-fixtures.py` tests these refusals. It also refuses any
OCaml source file in the repository outside `_build`, `.git` and
`.gatework*` directories. The Makefile target is `test-reference-fixtures`.

## Provenance

The fixtures use Kanon revision `69f3be5198cda4334de4fd23b4ccc92cac595789`.

- Checking, evaluation and erasure: the sha256 of each fixture log equals the
  `observations_sha256` value of the record at HEAD `2a22f67`.
- Pinfront, elaboration and program: the fixtures point to the existing
  reference snapshots. These snapshots are unchanged.
- Family-layout: the fixture equals the HEAD record.
- Core-erasure: the HEAD `stage-a-core-erasure.json` was stale since commit
  `36a28a4` and held 3 results. Thus 5 of the 8 values rest on raw adapter
  output. That output is the repository files
  `_build/core-erasure/oracle-{0..7}.stdout`, mtime 2026-10-01 17:35. The
  JSON record holds their sha256 values. The adapter sha256 is
  `f2067e5eda8732c0078f53debeef27d71f6c71af01c1838313c28bfd226295d8`. This
  value equals HEAD `test/core-erasure-oracle.ml`.
- Spot check: a reviewer ran `kanon.exe check` and `kanon.exe check --print`
  on program cases 0, 2, 83 and 122. All 8 runs equal the snapshot.

The original records came from a copy tree under
`/private/tmp/sole-comb-no-ocaml-edit`. The log paths in the JSON record point
to ephemeral copies outside the repository.

## Core-erasure record catch-up

This change also goes beyond the OCaml removal. The core-erasure records move
from stage A.5b.3.2a to A.5b.3.2d. The mutation record grows from 4 to 18
mutants.

## Review fixes

- F1: `dev/test-representation.py` no longer reads Kanon `lib/*.ml`. It
  holds the 8 digests of revision 69f3be51 as a constant. `dev/toolchain.json`
  no longer pins `kanon.exe`, and `dev/pin-check.py` no longer checks it. The
  `kanon.checkout.head` check stays, because `dev/test-pinfront.py` and the R2
  tools read the Kanon checkout. The full pin check now has 34 checks.
- F2: the loader refuses a reference snapshot when its case names and source
  hashes differ from the inputs. For the program scope, each observation must
  be an object.
- F3: the OCaml source scan covers the whole repository. New subtests cover
  the manifest, snapshot, byte and count refusals, and the F2 refusal.
- F4: the change removes the unused `parse_rows`, `ORACLE_NAMES` and
  `ORACLE_NAME_PATTERN`, and corrects the A.3 text in
  `dev/stage-a-milestones.md`.

## Review rerun

On 2026-10-01 the review reran the gates on a copy of the staged tree. The
copy kept `.git`. The run waited for a load average below 40 (120 s
of waiting). The run used `make gates` without test-bench and R2, one
target for each step. The program step ran the full 245-file corpus last.
The logs show these hosts: bun, node-worker, native.

| Step | Exit | Seconds |
| --- | --- | --- |
| house | 0 | 0 |
| fixtures | 0 | 0 |
| foundation | 0 | 86 |
| representation | 0 | 105 |
| evaluation | 0 | 175 |
| checking | 0 | 107 |
| pinfront | 0 | 620 |
| elab | 0 | 342 |
| erasure | 0 | 23 |
| cli3 | 0 | 223 |
| core | 0 | 206 |
| family | 0 | 93 |
| buildcheck | 0 | 2 |
| pincheck | 0 | 0 |
| program | 0 | 2555 |

Start: `22:30  28 users, load averages: 38.25 44.68 47.36`. End: `23:34  28 users, load averages: 132.67 115.01 88.53`.

In the first run, two steps exceeded fixed time limits while the load average
was above 100. The evaluation step exited 2 after 127 s: the Bend compile of
`checks.bend` exceeded its 120 s limit. The program step exited 2 after 1898 s:
`bun/0006-both` exceeded its 1800 s limit. In both steps, every check before
the timeout passed, including the fixture comparisons.

A second run of only these two steps used the same copy and the same index.
The evaluation step exited 2 after 129 s: the Bend compile exceeded
120 s again. The compiler child process finished the binary about 5 minutes
after the start. The program step exited 2 after 4932 s: bun and node-worker
passed 245 files in 2 modes, and then `emit-native` exceeded its 300 s limit.
The second run used the same limits as the first run.

The review then set the limit for the native Bend compile in
`dev/test-evaluation.py` and for `emit-native` in `dev/test-program.py` to
1800 s. 1800 s is the limit that `dev/test-program.py` already uses for large
inputs. All other limits did not change. A third run of only these two steps
started at 01:30:24, after 120 s of waiting for the load gate. The table
shows the third run for these two steps.
Third run start: `1:30  28 users, load averages: 39.94 58.67 68.20`. End: `2:15  28 users, load averages: 11.79 16.44 28.16`.
All steps passed. The program and program-mutation records come from this rerun.
