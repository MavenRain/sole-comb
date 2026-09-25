# S0-5 probe preparation

`r2-prepare.py` builds a scratch probe and checks its behavior before handing
artifacts to the [serial measurement runner](r2-run.md). It makes no timing
samples, chooses no endpoint, and writes no R2 verdict. Its checks can run
while the machine is above the benchmark load ceiling.

The scratch lexer/parser/checker and its two source twins are still required.
They belong outside this repository, under
`/private/tmp/claude/kan-elim-lang-m0/r2-risk/`. Preparation does not generate
a compiler or establish semantic equivalence between twins. Review the
chosen seed, translation, and rejection cases before using their timings.

## Preparation plan

Paths are absolute or relative to the plan file. The schema is strict:

```json
{
  "schema": 1,
  "probe": {"entry": "probe.bend"},
  "workloads": {
    "small": {"bend": "twins/small.bend", "sole": "twins/small.sole"},
    "conversion": {"bend": "twins/conversion.bend", "sole": "twins/conversion.sole"}
  },
  "empty": "twins/empty.sole",
  "rejects": {
    "lex": "rejects/lex.sole",
    "parse": "rejects/parse.sole",
    "type": "rejects/type.sole",
    "conversion": "rejects/conversion.sole"
  },
  "startup": {
    "bun": {
      "origin": "/Users/oobi/Documents/attest/_build/js/bin/attest.js",
      "input": "startup/trivial.att",
      "arguments": ["check"],
      "accept_line": "ACCEPT",
      "cache_state": "cold"
    },
    "node-worker": {
      "origin": "/Users/oobi/Documents/assay/_build/bend/assay.js",
      "input": "startup/trivial.asy",
      "arguments": ["check"],
      "accept_line": "ACCEPT",
      "cache_state": "cold"
    }
  }
}
```

The reference bundle paths above were located on 2026-09-25. The command
checks their existence and hashes on each run. Verify the argument prefixes,
acceptance lines, and trivial inputs against each reference compiler.
Preparation runs copied bundles from its scratch output directory. It
neither builds in nor modifies the reference checkouts.

The probe takes one input path as its sole argument. Accepted inputs must
exit 0 with an exact `ACCEPT` line on stdout. Each negative case must exit 1
with an exact `REJECT` line and no `ACCEPT` line on stdout. A crash, timeout,
or missing-file error does not satisfy that contract. Supply a separate
negative case for lexing, parsing, typing, and conversion. All four must
differ from one another and from accepted inputs. These labels describe
the supplied cases; the preparer cannot prove which compiler phase rejected
them. Strong cases should isolate their intended failure.

## Build and qualify

```sh
make r2-prepare \
  R2_PREPARE_PLAN=/private/tmp/claude/kan-elim-lang-m0/r2-risk/prepare-plan.json \
  R2_OUTPUT=/private/tmp/claude/kan-elim-lang-m0/r2-risk/prepared-001

make r2-run \
  R2_PLAN=/private/tmp/claude/kan-elim-lang-m0/r2-risk/prepared-001/run-plan.json \
  R2_OUTPUT=/private/tmp/claude/kan-elim-lang-m0/r2-risk/attempt-001
```

Both output directories must be new and outside project and tool checkouts.
`R2_TOOLCHAIN` selects the same pin file for both commands. The Python CLI
returns 0 for READY, 1 for errors or failed checks, 3 for a required child
deadline, and 128 plus the signal number for cancellation. A harness error
on a candidate check, such as a failed spawn, gives 1 and outranks every
other result. On one endpoint, a failed check outranks a deadline. The result is 3
when a deadline stops a candidate that has no failed check, even if the
other candidate failed. `make` reports
its own nonzero exit status; use the Python CLI when exact codes matter.

Preparation performs these steps serially:

1. Bind the plan, pins, probe import closure, twin import closures, empty
   input, rejection cases, and reference bundle inputs to their hashes.
2. Check the pinned tools and repositories. Compile JavaScript and attempt
   native output using the pinned Bend compiler, scratch `BEND_LIB` cache,
   and `BEND_NO_TELEMETRY=1`. `SOLE_COMB_CC`, then `CC`, then local clang
   selects the informational native C compiler.
3. Check both Bend twins with `--check-only`, requiring exit 0 and the exact
   `All terms check.` stdout line.
4. Run both JavaScript endpoints on both twins, the empty input, and every
   rejection case. A candidate that fails a check (a wrong verdict, exit
   code, or output line) is recorded as false in `qualified_candidates` and
   in the `disqualified` list, and it is dropped from selection. A harness
   error on a candidate check does not drop the candidate. It stops
   preparation with ERROR, and no run plan is published. Otherwise,
   preparation fails only when no candidate qualifies.
5. Qualify native output if its build succeeded. Native failure, timeout,
   or a wrong verdict is recorded as informational unavailability. Native
   never becomes an endpoint candidate.
6. Check the copied reference bundles on their startup inputs, recheck
   pins, and validate the handoff with the collector before publication.

Every child has the pinned build or leg deadline and separate stdout and
stderr logs. Timeout or cancellation terminates its process group and waits
for the child. A signal that arrives while a child starts or while its group
is cleaned up is delivered after the group is killed, and it cancels
preparation. SIGINT, SIGTERM, and SIGHUP preserve the partial journal.
Changing a tracked input between checks fails preparation.

## Output and evidence

`preparation.json` records commands, exit codes, deadlines, artifact hashes,
all logs, and qualification results. Each build also writes
`logs/build-<backend>.log`, its stdout followed by its stderr, because the
pinned compiler reports on stderr. The run plan hands off that combined file
as `build_log`. An informational native failure has
`native-unavailable.json` with build logs and any qualification checks.
`candidate-plan.json` and `contract-check/` retain the collector's contract
validation. They are diagnostic artifacts, not the published handoff. On
every exit path, `candidate-plan.json` holds the checked plan inside a
diagnostic envelope that the collector refuses as a run plan.

Only READY preparation publishes `run-plan.json`. Its `preparation` field
binds it to the journal, and its `disqualified` field lists the dropped
candidates. The collector verifies the plan and toolchain hashes,
qualification status, the `disqualified` list, and every recorded file,
including negative cases and build artifacts, before creating a measurement
attempt. It still measures both candidates, and the assembler never selects
a dropped one. Keep the prepared directory and its inputs in place and
unchanged through collection. The original manually assembled run-plan
format remains supported. A manual plan cannot drop a candidate.

Startup `cache_state` remains a declaration by the operator. These untimed
checks can warm caches; establish the declared state again before collecting
samples. Preparation neither clears caches nor certifies a cold sample.

`make test-r2-prepare` checks failure paths, stale handoffs, and actual
process-group timeout cleanup. It uses synthetic compiler responses for
orchestration tests and supplies no performance evidence.
