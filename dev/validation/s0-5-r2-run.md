# S0-5 serial measurement runner

Validated 2026-09-25 against base `ec0a7a237f085adb3135623ccff78f5d9188eaaf`.

The new `dev/r2-run.py` binds a prepared scratch probe, source twins, build
logs, and copied startup bundles, collects serial measurements, and invokes
the existing R2 assembler. The Makefile includes its regressions in `test`
and `gates`. Usage and input-plan fields are documented in `dev/r2-run.md`.

Change from the plan: the runner copies the startup bundles to
`<attempt>/startup/`, not to the plan's shared
`/private/tmp/claude/kan-elim-lang-m0/startup/`, so each attempt binds its
own hashed copy and no attempt overwrites another. `dev/r2-risk.md` and
`dev/r2-run.md` state the same location.

## Validation

`make gates` ran in `/Users/oobi/Documents/sole-comb` and exited 0.
Full output is retained in `s0-5-r2-run-gates.stdout` and
`s0-5-r2-run-gates.stderr` beside this record. The capture was
refreshed after review fix round 1 by a run of `make gates` from the repository
root. Each table row comes from that gates capture, except a row that names its own
command and capture file.

| Check | Result |
| --- | --- |
| Benchmark harness regressions | 26 passed |
| Build cache regressions | 4 passed |
| Pin-check regressions | 3 passed |
| R2 evidence assembler regressions | 39 passed |
| Serial collector regressions | 31 passed |
| Toolchain checks | 35 passed |
| Build Bend pin checks | 6 passed |
| Bend host entry check | Passed |
| `git diff --cached --check` (run separately, not part of `make gates`) | exit 0, see `s0-5-r2-run-diff-check.txt` |

The collector tests pass generated child reports through the real evidence
validator and assembler. They cover AMBER, independently confirmed GREEN,
FAIL, a degraded confirmation, changed endpoint selection, native-only
information, a native leg that is UNMET on a thread-count miss or a child
deadline (recorded, collection continues) and on a load reason (collection
stops), child rejection, harness failure, an INTERRUPTED harness report
(collection stops with exit 130), a missing report (the leg is ERROR),
source and pin mutation, literal argv, a startup accept line with a leading
dash that reaches the harness, import binding, new-output refusal,
refusal of an output in the project, prior-art, Bend or kanon checkout,
refusal of probe sources in the project, and preparation failure.
A separate process test sends SIGINT only to the collector and verifies
that the real benchmark harness reaps its measured child before exit.
Another test verifies that the harness runs in its own process group.

These are correctness tests. Their generated timing samples are synthetic
and do not provide an R2 performance result.

## Measurement state and next work

The live preflight in `s0-5-r2-run-preflight.json` used the repository's
unchanged toolchain pins and passed its pin check. At
`2026-09-25T17:23:39.536711+00:00`, load1 was `43.12109375` against the `8.0`
ceiling, so the result was `NOT_READY` (exit 3). No timed benchmark leg or
3600-second load wait was started. This is a historical observation, not a
claim about a future run's load.

The next implementation work remains the scratch Bend lexer/parser/checker
and faithful small and conversion source twins under
`/private/tmp/claude/kan-elim-lang-m0/r2-risk/`. Build the Bun/Node probe
bundle, record the native build attempt, locate and validate the two
prebuilt startup bundles and trivial inputs, then collect on a quiet host
using a new attempt directory. The runner now handles serial collection,
GREEN confirmation, and evidence assembly once those assets are ready.

No `dev/r2-risk.json` result has been produced and the pinned endpoint
remains null. Stage A is still pending the S0-5 evidence and plan rules.
