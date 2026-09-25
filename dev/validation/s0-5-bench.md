# S0-5 measurement harness, 2026-09-24

This change completes the measurement-harness prerequisite for the R2-RISK
probe. S0-5 itself is still incomplete; there is no ratio or selected endpoint.

The original shell harness sampled load only before a child, combined user and
system CPU, and could leave a subprocess running after its shell timed out.
The Python harness adds before/during/after load rejection, separate child CPU
accounting, thread observations, process-group deadlines and retained JSON
results. A failed or incomplete leg cannot publish a median.

Validation on the installed repository:

- `make gates`: exit 0. All 29 regression tests pass (22 harness tests in
  `dev/test-bench.py`, 4 build tests in `dev/test-build.py` and 3 pin-check
  tests in `dev/test-pin-check.py`), all 35 toolchain pin checks pass, and
  Bend checks `bin/sole-comb.bend` successfully.
- The timeout regression spawns a descendant that would write a marker after
  its parent is killed. The marker is absent after its scheduled write time.
- Load regressions cover a busy machine, recovery, the ceiling boundary,
  mid-run spikes (the `measure()` observer peak and its non-finite error),
  post-run spikes, bounded retries and non-finite observations.
- A timeout stays UNMET (exit 3) when darwin `killpg` returns EPERM for a
  process group of zombies. The thread count test checks the value: 1 for a
  single-threaded child, 2 or more for a child with a second thread.
- Verdict regressions cover nonzero child exit, missing acceptance output,
  missing thread observations, pin failure and partial runs without medians.
- `zsh -n dev/bench.sh` and `git diff --cached --check` pass.

Review fixes, 2026-09-25:

- `dev/build.py` hashes indented header imports and foreign
  `import "<path>.c|js"` files, so an edit to them invalidates the cache.
- The Node worker launcher resolves the bundle path, so a bare relative
  invocation such as `_build/endpoint/node-worker` runs.
- `native` is INFO only. `pin-check` fails when it is the endpoint or one of
  `endpoint_candidates`, and `build.py` refuses to activate it.
- `bench.py --argv` runs a leg without a shell. The review probe (31 runs each,
  load1 13.5) measured a median `wait4` CPU of 1.395 ms for `/usr/bin/true`
  direct and 4.320 ms for `zsh -f -c 'exec /usr/bin/true'`. The shell path
  keeps that offset, and its `cpu_method` says so.

Full gate output is in `s0-5-bench-gates.txt`; the regression transcript is in
`s0-5-bench-tests.txt`.

The preflight in `s0-5-bench-preflight.json` verifies the pins and records load
7.8452 against ceiling 8.0. It returns `READY` (exit 0), with zero samples.
It is a single observation, not an R2 verdict, and it does not claim that the
3600-second load-wait budget elapsed. Pin limits and
the endpoint remain as recorded in `dev/toolchain.json`.

Next: prepare the scratch Bend subset checker and both twins, build the endpoint
candidates and native informational probe, then run the S0-5 comparisons and
prebuilt-bundle start-up legs on a quiet machine. The required GREEN repeat and
per-workload results must precede `dev/r2-risk.json` and endpoint selection.
The kernel port has not started.
