# sole-comb

A language with Kan extensions as its type-forming primitives and one primitive
sum eliminator. The compiler host is Bend 2; the intended target is WebAssembly.

The repository currently contains the Stage 0 host skeleton, toolchain pins and
benchmark infrastructure. The kernel port has not started. The build plan is
`../kan-elim-lang-m0/M0-PLAN.md`; the next stage is its S0-5 R2-RISK probe.

Run from this directory with the locally pinned tools:

```sh
make build           # cached JavaScript build of the host entry
make check           # Bend checks the host entry
make test            # build, benchmark regressions and pin checks
make gates           # harness, build and pin-check regressions, pin checks, host check
make bench-preflight # pins and current load; no benchmark samples
```

`dev/toolchain.json` retains `endpoint: null` until S0-5 selects Bun or the Node
worker using valid measurements. Consequently `make build` creates candidate
launchers under `_build/endpoint/` and does not activate `./sole-comb` yet.
No command installs tools or updates the pinned Bend checkout.

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

## Remaining Stage 0 work

The measurement harness is a prerequisite for S0-5. S0-5 still needs the scratch
Bend lexer/parser/checker and its two source twins, the native informational
build, Bun and Node worker measurements, prebuilt-bundle start-up measurements,
the GREEN repeat when required, and `dev/r2-risk.json`. Scratch compiler code
belongs under `/private/tmp/claude/kan-elim-lang-m0/r2-risk/`, outside this tree.

The plan's load ceiling is 8.0 with a 3600-second wait budget. Endpoint selection
and the Stage A kernel port remain pending until the probe has valid evidence.
The current preflight observation is recorded in
`dev/validation/s0-5-bench-preflight.json`; it is a historical observation, so
rerun `make bench-preflight` before measuring.
