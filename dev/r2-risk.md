# S0-5 R2 evidence assembly

`make r2-report R2_MANIFEST=/absolute/path/to/manifest.json` verifies saved
benchmark reports and writes `dev/r2-risk.json`. It does not build the scratch
checker, run measurements, edit the endpoint pin, or open Stage A. Scratch
compiler code stays under `/private/tmp/claude/kan-elim-lang-m0/r2-risk/`.

Use `make test-r2` for the assembler's synthetic regression tests. Synthetic
fixtures are temporary and never become project benchmark results.

## Measurements to collect

Use `dev/bench.py` from this revision, with `--argv`, `--require-threads`,
`--runs 5`, `--accept-line`, `--input FILE`, and `--json`, for every leg. Paths in commands
must be absolute and resolved, including `/private/tmp` on macOS. Do not
override the pinned load ceiling or change the denominator. Benchmark reports
now include `completed_at`, allowing the assembler to reject overlapping legs.
Old reports without completion timestamps or input snapshots need new measurements.

Repeat `--input` for the executed bundle or native binary, its input, and
every scratch probe source dependency. For the denominator, include the Bend
twin and its imports. List those imports in the workload's `bend_imports`.
The assembler follows the twin's `.bend` imports and foreign `.c`/`.js`
imports the same way as `dev/build.py`. The list must match that closure
exactly, and each denominator leg must snapshot every import once. The
pinned Bend binary covers `import Base`. For a prebuilt startup leg, include the copied bundle
and its trivial input. The harness hashes these files before and after the
leg and rejects changes. The assembler binds these snapshots to the manifest,
so an old measurement cannot be relabeled with a different source or bundle.

The scratch probe's CLI contract is one input path and a complete `ACCEPT`
stdout line on successful checking. The Bend denominator's acceptance line
is `All terms check.`. Keep the compiler's build log and all scratch source
dependencies, including foreign imports. The small and conversion workloads
must be distinct. Review the twins and conversion laws before measuring:
the assembler verifies evidence consistency, not semantic equivalence or
checker soundness.

Each round contains two denominator legs and three probe legs per JavaScript
endpoint: small, conversion, and empty. The empty probe measures its startup
share independently of the full-size prebuilt bundles. Commands are checked
against the pinned paths and stack configuration:

| Leg | Direct argument vector |
| --- | --- |
| Denominator | pinned Bend, Bend twin, `--check-only` |
| Bun probe | pinned Bun, scratch probe.js, sole-comb twin |
| Node probe | pinned Node, `-e`, `build.node_worker_script(pins)`, scratch probe.js, twin |
| Native probe, informational | scratch native binary, twin |

For prebuilt startup legs, copy the existing `attest.js` and `assay.js` bundles
to a scratch directory outside the attest and assay trees; `dev/r2-run.py`
copies them to `<attempt>/startup/`. Never build in those source
trees. Their source and copied hashes must match. Collect one startup leg for
each runtime on its own trivial input. `arguments` below are CLI arguments
between the bundle and input path, such as `check` or `--check`, as required
by that compiler. Set its actual acceptance line. Use the same Bun or Node
worker launcher shown above.

Establish and declare the startup compile-cache state with `--cache-state
cold` or `--cache-state warm`. That harness option records a declaration;
it does not reset or populate a cache. The assembler checks that every
accepted startup sample has the declared state. It cannot independently
verify how an external runtime's cache was prepared.

Native C is informational. Supply its binary, build log and three probe
reports, or supply the failed native build's log as `unavailable`. Native
results never participate in runtime selection.

## Manifest format

Every `REF` below is an object with exactly `path` and `sha256`. Paths may be
relative to the manifest; command paths inside benchmark reports must use
the resolved absolute form. Hash the files after measurement with SHA-256.
All reports, sources, binaries, bundles and logs are checked before assembly
and checked again before a complete result is published.

```json
{"path": "/private/tmp/claude/kan-elim-lang-m0/r2-risk/small.sole", "sha256": "64 lowercase hex digits"}
```

The following schematic uses `REF` strings for those objects. Replace every
one with a real reference; the literal strings are not valid input.

```json
{
  "schema": 1,
  "probe": {
    "sources": ["REF to probe.bend and each dependency"],
    "javascript": "REF to probe.js",
    "build_log": "REF to the JavaScript build log"
  },
  "workloads": {
    "small": {"bend": "REF", "sole": "REF", "bend_imports": ["REF to each import"]},
    "conversion": {"bend": "REF", "sole": "REF", "bend_imports": []}
  },
  "empty": "REF to the empty probe input",
  "startup": {
    "bun": {
      "origin": "REF to attest's existing prebuilt attest.js",
      "bundle": "REF to its scratch copy",
      "input": "REF to a trivial attest input",
      "arguments": ["check"],
      "accept_line": "ACCEPT",
      "measurement": "REF to the startup bench report"
    },
    "node-worker": {
      "origin": "REF to assay's existing prebuilt assay.js",
      "bundle": "REF to its scratch copy",
      "input": "REF to a trivial assay input",
      "arguments": ["check"],
      "accept_line": "ACCEPT",
      "measurement": "REF to the startup bench report"
    }
  },
  "native": {"unavailable": "REF to a failed native build log"},
  "rounds": [{
    "denominator": {"small": "REF", "conversion": "REF"},
    "endpoints": {
      "bun": {"small": "REF", "conversion": "REF", "empty": "REF"},
      "node-worker": {"small": "REF", "conversion": "REF", "empty": "REF"}
    }
  }]
}
```

A built native record instead has exactly `binary`, `build_log` and
`measurements`, where `measurements` has the same small/conversion/empty
shape as an endpoint. A confirmation round has the same shape as the first
round and contains fresh reports measured after that round finished. Startup
and native measurements are shared between rounds. All captured legs must
run serially.

## Decisions and output

Ratios use the median child user-plus-system CPU from exactly five accepted
timed samples. Warm-up and discarded load attempts cannot affect the median.
The assembler recomputes medians from samples rather than trusting summaries.
It retains full leg evidence, artifact hashes, current pin-check results,
the assay prior (1.346 with a different denominator), and startup shares.

An endpoint must accept every probe input. A recorded child failure or
verdict mismatch drops that candidate. An `ERROR` report is a drop only
when its reason is `child exit or acceptance line mismatch`, no sample timed
out, and the last sample shows the failure. Any other `ERROR` needs a new
measurement. Unmet measurement conditions cannot be used to drop a
competitor and select the remaining runtime. An optional manifest
`disqualified` list, a strict subset of the candidates in candidate order,
drops each listed candidate in every round, whatever its legs show. Missing or
failed denominators and startup legs prevent a complete result.

The chosen candidate must have the lowest median CPU on both workloads.
If they disagree, the result is UNMET pending a user selection rule. Exact
ties prefer Bun. These tie and crossover rules make an ambiguity in the
plan explicit; no average across workloads is invented. Switching an already
pinned endpoint also requires a ruling.

The plan-derived thresholds are GREEN at ratio <= 0.45, AMBER at ratio <= 1,
and FAIL above 1. The worst workload controls the result. A first GREEN
requires a second round because of the contrary assay prior. A repeated
result uses the worse ratio across both rounds. A change of winner prevents
selection. The threshold status is recorded as not user-ratified.

Exit codes are 0 for complete GREEN or AMBER, 4 for complete FAIL, 3 for
UNMET, 1 for invalid evidence or pins, 2 for CLI usage, and 130 for operator
interruption. Only COMPLETE has a selected endpoint and verdict. FAIL keeps
Stage A halted under the plan. A report never edits `dev/toolchain.json`.

The report's `pins` block copies the Bend revision, version, binary and
`binary_sha256`, and the path, realpath, version and `sha256` of Bun and
Node. After the pin check passes, `pins.observed` records the sha256 of the
Bend, Bun and Node files as read now. A difference from a pin is an error.
The R2-RISK gate compares this block with M0-PLAN section 0.1.

`toolchain.sha256` and each leg's `toolchain_sha256` bind the legs to
`dev/toolchain.json` as it was when they were measured. The endpoint pin
changes that file. After the pin, a re-assembly of the same legs must pass
`--toolchain` a saved copy of the pre-pin file, or set
`R2_TOOLCHAIN=/absolute/path/to/pre-pin-toolchain.json` for `make r2-report`.

Reports are replaced atomically, and a requested output path that aliases
an input is refused. Failed assembly retains partial evidence with a null
endpoint and verdict. A failed rerun never replaces a COMPLETE report: it
writes the sibling `<output stem>.failed.json` (for the default output,
`dev/r2-risk.failed.json`), keeps the nonzero exit code, and prints that
path. A rerun that would write that sibling over an input writes nothing. Run `make gates` after changes to this tooling.
