# S0-5 measurement collection

`r2-run.py` runs the S0-5 legs serially through `bench.py` and feeds their
evidence to [the R2 assembler](r2-risk.md). It consumes a prepared scratch
compiler, its source twins, and existing startup bundles. It does not build
that compiler, install tools, modify the reference checkouts, or activate a
runtime endpoint.

The scratch lexer/parser/checker, faithful small and conversion twins, and
their build logs still need to be written before real collection can run.
Runner tests use synthetic measurements and do not establish an R2 verdict.

## Input plan

Paths are absolute or relative to the plan file. The format uses paths
rather than precomputed hashes; preparation binds each file to its current
SHA-256. The following example shows the required shape. Replace the bundle
locations with the actual prebuilt files in the pinned reference checkouts.
Their locations remain unverified.

```json
{
  "schema": 1,
  "probe": {
    "sources": ["probe.bend"],
    "javascript": "probe.js",
    "build_log": "build-javascript.log"
  },
  "workloads": {
    "small": {"bend": "twins/small.bend", "sole": "twins/small.sole"},
    "conversion": {"bend": "twins/conversion.bend", "sole": "twins/conversion.sole"}
  },
  "empty": "twins/empty.sole",
  "startup": {
    "bun": {
      "origin": "/Users/oobi/Documents/attest/PREBUILT_LOCATION/attest.js",
      "input": "startup/trivial.att",
      "arguments": ["check"],
      "accept_line": "ACCEPT",
      "cache_state": "cold"
    },
    "node-worker": {
      "origin": "/Users/oobi/Documents/assay/PREBUILT_LOCATION/assay.js",
      "input": "startup/trivial.asy",
      "arguments": ["check"],
      "accept_line": "ACCEPT",
      "cache_state": "cold"
    }
  },
  "native": {"unavailable": "build-native-failed.log"}
}
```

List every local dependency of the probe in `probe.sources`, including
foreign imports. The runner discovers and binds the Bend denominator's
local import closure automatically. Base-library imports are covered by
the pinned Bend checkout. The two workload files must differ on each side;
the empty input must contain only whitespace. The collector checks file
identity and measurement evidence. It cannot establish that the two source
languages express the same program or that the probe implements its syntax
correctly. Validate those properties before collection.

Record the actual native build attempt. If it succeeds, use this shape
instead of `unavailable`:

```json
"native": {"binary": "probe-native", "build_log": "build-native.log"}
```

The native leg is informational. It never enters endpoint selection.
If a native leg ends UNMET because the harness saw no child thread count
or the child passed its deadline, the runner records that leg and
collection continues. A native leg that ends UNMET for a load reason, is
cancelled, or has a harness error still stops collection.
For startup, supply the actual argument prefix and exact acceptance line
of each reference compiler. The input path is appended to those arguments.
The runner copies the prebuilt bundle into the new attempt directory and
requires its bytes to match the origin throughout collection. This
`<attempt>/startup/` copy replaces the plan's shared
`/private/tmp/claude/kan-elim-lang-m0/startup/` directory: each attempt
binds its own hashed copy, and no attempt overwrites the bundle of another
attempt.

`cache_state` is a declaration of the prepared environment, either `cold`
or `warm`. The runner passes it through to the harness; it does not clear
runtime caches or create a warm cache. Establish the declared state for
every sample using the reference runtime's settings before measuring.

## Prepare and collect

Prepare an inspectable manifest without starting tools or samples:

```sh
python3 -P dev/r2-run.py \
  --plan /private/tmp/claude/kan-elim-lang-m0/r2-risk/run-plan.json \
  --output /private/tmp/claude/kan-elim-lang-m0/r2-risk/prepare-01 \
  --prepare-only
```

This produces `manifest.json`, `collection.json`, and the startup bundle
copies. `PREPARED` means the input checks passed; toolchain pin checks occur
when `bench.py` runs. A prepared manifest has no measurements and is not an
R2 report. To collect, choose a different, new directory:

```sh
make bench-preflight
make r2-run \
  R2_PLAN=/private/tmp/claude/kan-elim-lang-m0/r2-risk/run-plan.json \
  R2_OUTPUT=/private/tmp/claude/kan-elim-lang-m0/r2-risk/attempt-01
```

`R2_TOOLCHAIN` selects the toolchain file for both collection and assembly.
Each attempt directory must be new and outside sole-comb and all reference
and tool checkouts. Completed and interrupted directories cannot be reused.
All supplied inputs and prior reports remain bound to their initial hashes.

The serial order is startup Bun, startup Node worker, available native
measurements, then denominator and endpoint measurements for the first
round. Each probe endpoint runs small, conversion, and empty inputs. Each
leg uses direct argv, the literal pinned Node worker when applicable,
thread observations, one warm-up and exactly five timed samples. The load
ceiling, wait budget, and child deadline come from the toolchain pins.

The assembler's decision rule determines whether a fresh second round is
required: GREEN always gets an independent confirmation. A changed winner
remains UNMET. A GREEN result can degrade to AMBER or FAIL on confirmation.
Only an evidenced child rejection can drop a candidate endpoint. Harness
errors, changed inputs, unmet measurement conditions, and cancellation stop
collection and preserve the attempt. The one exception is an informational
native leg that ends UNMET on a missed child thread count or a child
deadline: the runner records it and collection continues. Interrupting the
collector forwards SIGINT to the harness so it can reap its measured child.

`collection.json` records the attempt status and each child invocation.
A leg with a missing or invalid report has the status ERROR.
`measurements/` retains raw successful, failed, and interrupted reports.
After collection finishes, the existing assembler writes `r2-risk.json`
under the attempt directory. Exit codes are 0 for preparation or a complete
GREEN/AMBER result, 4 for a complete FAIL result, 3 for unmet conditions,
1 for invalid inputs or evidence, and 130 for cancellation. Check the status
field as well as the exit code.

After reviewing a complete result, publish it into the repository with:

```sh
make r2-report R2_MANIFEST=/private/tmp/claude/kan-elim-lang-m0/r2-risk/attempt-01/manifest.json
```

The collector leaves `dev/toolchain.json` unchanged. The S0-5 plan's runtime
selection, FAIL ruling, and Stage A prerequisites still apply.
