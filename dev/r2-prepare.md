# S0-5 probe preparation

`r2-prepare.py` builds a scratch probe and checks its behavior before handing
artifacts to the [serial measurement runner](r2-run.md). It makes no timing
samples, chooses no endpoint, and writes no R2 verdict. Its checks can run
while the machine is above the benchmark load ceiling.

The [scratch lexer](validation/s0-5-r2-lexer.md) has passed its byte-token
correctness checks. The [scratch parser](validation/s0-5-r2-parser.md) has
passed its syntax-tree and refusal checks for the documented subset.
The [scratch name resolver](validation/s0-5-r2-resolver.md) has passed 76
exact-tree and refusal cases on Bun, the pinned Node worker, and the
informational native build. This is the first checker pass.

The [scratch conversion engine](validation/s0-5-r2-conversion.md) has passed
107 exact-normal-form, comparison, and refusal cases on those three runtimes,
with twenty detected mutations. It implements capture-safe normalization and
alpha/beta/delta/zeta conversion for the resolved function fragment.
The conversion engine is untyped. The
[scratch typechecker](validation/s0-5-r2-typing.md) supplies function-fragment
typing checks and has passed 96 exact signature and refusal cases on all
three runtimes, with nineteen detected mutations. It checks dependent
functions, annotations, `let` bindings, exact universes, and impredicative
`Prop` products through a combined source entry point.

The [scratch arm expander](validation/s0-5-r2-arms.md) has passed 57
exact-tree and refusal cases on all three runtimes, with twelve detected
mutations. It validates positional labels against supplied constructor
names and expands a final `else` without rewriting branch bodies.

The [scratch data checker](validation/s0-5-r2-data.md) has passed 40 exact
signature and refusal cases on all three runtimes, with nine mutations that
Bun detects. It checks non-indexed data types from caller metadata,
constructor spines in checking mode, and `elim` with an explicit motive, and
it applies K1 iota.

The [S0-5 closeout](s0-5-closeout.md) sets the remaining scratch scope.
The [two complete source twins](validation/s0-5-r2-twins.md) now have a
declaration map and passing lexical checks. Step 2 must meet the source
requirements in the twins record, including indexed families,
source-derived data metadata, scratch U32 literals, zero-arity
sum/prod/tuple with the zero-arm elimination of `sum ()`, K1/K1d recursion,
and Id/reflexivity checking. Portable file input remains a step 3 item.
Records, projections, `else` expansion with recursive sugar processing, and
motive inference are deferred. By reading, the step 1 translations need no
eta or proof-irrelevance rules, so these rules remain deferred. Scratch
sources belong outside this repository, under
`/private/tmp/claude/kan-elim-lang-m0/r2-risk/`. Lexer, parser, resolver,
conversion, typing, arm expansion, and data fixtures are not qualified probe
workloads.
Preparation does not generate a compiler or establish semantic equivalence
between twins. Review the chosen seed, translation, and rejection cases
before using their timings.

The active work order is the [S0-5 closeout](s0-5-closeout.md), using the
[frozen workload manifest](r2-workloads.json). It replaces the feature
scope and ordering in earlier scratch checkpoints. Step 1 supplies the
complete translations and their reviewed declaration map. Step 2 must
check both inputs and their negative fixtures from source, preserving all
four core obligations.
The manifest records selected Bend inputs and unqualified sole translations.
Source checker qualification, preparation, and measurements remain pending.
It is not a `prepare-plan.json` or a
qualification result; construct the strict preparation plan below after
the missing artifacts exist.

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
