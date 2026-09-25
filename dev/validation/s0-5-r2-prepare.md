# S0-5 probe preparation, 2026-09-25

Base: `fbc7e2dc6d06a5bfffa0c9403dd0abe13ed45fc8`.
The final gate reran in place in `/Users/oobi/Documents/sole-comb`.

## Delivered

`dev/r2-prepare.py:110` validates a preparation plan and binds its inputs;
`:191` runs children with separate logs and bounded lifetimes; `:266`
builds JavaScript and informational native artifacts, checks both twins,
qualifies each candidate with positive and negative cases, records and
drops a failing candidate, fails only when no candidate qualifies, and
publishes the handoff. The command is documented in `dev/r2-prepare.md`.

`dev/r2-run.py:79` rejects a generated plan if its READY record, toolchain,
plan bytes, input files, or artifacts have changed. Existing manual plans
remain supported. No endpoint pin or measurement rule changed. The review
fixes also changed the committed `dev/r2-risk.py`, `dev/r2-run.py`, and
`dev/r2-risk.md` to accept an optional manifest `disqualified` list.

## Validation

Final `make gates`: exit 0. Full streams are
`s0-5-r2-prepare-gates.stdout` and `s0-5-r2-prepare-gates.stderr`.
Rerun in place in /Users/oobi/Documents/sole-comb after review fixes, 2026-09-25.

| Check | Result | Evidence |
| --- | --- | --- |
| Benchmark regressions | 26 passed | gates stderr:29 |
| Build regressions | 4 passed | gates stderr:38 |
| Pin-check regressions | 3 passed | gates stderr:46 |
| R2 evidence regressions | 40 passed | gates stderr:91 |
| Serial runner regressions | 32 passed | gates stderr:96 |
| Preparation regressions | 38 passed | gates stderr:101 |
| Full pins | 35 passed | gates stdout:10 |
| Bend pins and host check | 6 passed, build check OK | gates stdout:12, stdout:14 |

The 40 new tests (38 preparation, 1 R2 evidence, 1 serial runner) include
actual process-group timeout cleanup and actual SIGINT, SIGTERM, and SIGHUP
handling. Compiler response fixtures exercise failed builds, false
acceptance, crash versus rejection, startup failure, source mutation,
protected paths, and stale preparation records (`dev/test-r2-prepare.py`).
After the review fixes, the tests also cover these cases. One failed
candidate is recorded and dropped, and a disqualified endpoint never wins
or drives the confirmation round. EPERM from `killpg` on a zombie group
keeps the timeout and the cancellation. `candidate-plan.json` cannot load
as a plan. A signal during deadline cleanup or at spawn still cancels and
kills the group, and no handler stays at SIG_IGN. A definitive candidate
failure outranks a deadline on another row. The build log hands off both
stdout and stderr. A harness ERROR on a candidate stops preparation,
and only failed checks disqualify a candidate. The full test total is
143.

## Pinned build smoke

`s0-5-r2-prepare-build-smoke.json` retains the real build and execution
record. The pinned Bend compiler built the copied host entry as JavaScript
and native output. Bun, the Node worker, and native each printed `42` and
exited 0. Pin checks passed before and after. Capture:
`/Users/oobi/Documents/gpt5/sole-comb-r2-prepare/.kanon-exec/run-ezT3RM`.
Scratch record:
`/private/tmp/claude/kan-elim-lang-m0/r2-prepare-smoke-4nbizqge/build/preparation.json`.

This was a constant-returning host fixture, not the R2 lexer/parser/checker
or an R2 workload. Its record is marked `BUILD_SMOKE_ONLY`. It did not run
the fixture's dummy twin inputs, qualify a probe, publish a run plan, or
produce performance evidence.

## Remaining S0-5 work

The scratch lexer/parser/checker, reviewed source twins and rejection cases,
actual startup-input qualification, and benchmark collection are pending.
The existing full-compiler prebuilt bundles were located at the paths in
`dev/r2-prepare.md`; they were read and hashed for the smoke setup, not run.

`s0-5-r2-prepare-preflight.json` records a passing pin check and load1
14.99951171875 against ceiling 8.0. Its status is `NOT_READY`, with no
samples. This preflight did not spend the 3600-second load wait budget.
No R2 verdict, endpoint selection, or Stage A kernel port is claimed.
