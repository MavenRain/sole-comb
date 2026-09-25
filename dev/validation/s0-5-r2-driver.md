# S0-5 R2 evidence assembler, 2026-09-25

The next Stage 0 infrastructure increment is implemented. S0-5 itself remains
incomplete: the scratch checker and its twins have not been built, no R2
measurements have been collected, and the endpoint pin remains null.

Base: `96945abb2770f67053979cd9f666f1927df2f740`.
Plan: `../kan-elim-lang-m0/M0-PLAN.md`, S0-5, lines 160 through 178.
Plan SHA-256: `075f94656612099a040d353f2d52ef858e02834def330ece7ed4651c43268fa3`.

Implemented `dev/r2-risk.py`, its manifest contract in `dev/r2-risk.md`, and
`make r2-report` / `make test-r2`. The assembler binds measurements to pins,
commands, source and bundle hashes, startup cache declarations, five accepted
samples, serial leg timestamps and independent GREEN confirmation evidence.
It recomputes child CPU medians, retains native results as informational, and
leaves crossed workload winners or endpoint switches unresolved. The benchmark
harness now accepts repeated `--input` paths, refuses changed inputs, and
records a completion timestamp.

Validation on the installed files in `/Users/oobi/Documents/sole-comb`:

| Check | Result |
| --- | --- |
| `make gates` | exit 0 |
| Benchmark regressions | 26 passed |
| Build regressions | 4 passed |
| Pin-check regressions | 3 passed |
| R2 assembler regressions | 39 passed |
| Combined regressions | 72 passed |
| Pinned tools and limits | 35/35 checks passed |
| Bend compiler identity | 6/6 checks passed |
| Bend host entry check | passed |

Review fixes, round 1: The denominator legs now bind the Bend twin imports.
A missing or changed import makes the assembler refuse the leg (F1). The
report records the pinned binary SHA-256 values. A failed rerun writes the
`<output stem>.failed.json` sibling and keeps the COMPLETE record (F3). The
assembler drops an ERROR leg only when the reason is a child failure (F4). The
tests now reach the input binding and the ratio arithmetic: fixtures use
distinct timed values and per-workload denominators, and eight mutants fail
(F5, F6).

Review fixes, round 2: The failed-run sibling name follows the output stem,
and the refusal names the path that would overwrite an input.
`make r2-report` takes `R2_TOOLCHAIN` for a saved pre-pin toolchain file.
Fixture legs snapshot exactly their required inputs. New regressions pin the F4
reason clause, per-round denominators, a stale `bend_imports` ref and the
failed-run sibling; each fails on its mutant or on the pre-fix code.

Full gate output is retained in `s0-5-r2-gates.stdout` and
`s0-5-r2-gates.stderr`. The original managed capture is
`/Users/oobi/Documents/gpt5/.kanon-exec/run-odwRHb`.
Regression measurements are synthetic fixtures, not R2 evidence.

The preflight at 2026-09-25 09:25:30 UTC returned exit 3, NOT_READY:
one-minute load 10.06201171875 exceeded the pinned ceiling of 8.0.
Pins passed. `s0-5-r2-preflight.json` preserves this observation. It contains
no timed samples and does not establish that the 3600-second wait budget
was exhausted.

Next: implement and review the scratch lexer/parser/checker and the small
and conversion-heavy twins under `/private/tmp/claude/kan-elim-lang-m0/r2-risk/`.
Then build the JavaScript and informational native probe, establish cache
states, collect serial legs under valid load, and assemble `dev/r2-risk.json`.
The prebuilt startup bundles were located at
`/Users/oobi/Documents/attest/_build/js/bin/attest.js` and
`/Users/oobi/Documents/assay/_build/bend/assay.js`; copy them to the plan's
startup scratch directory without rebuilding those repositories. Kernel
porting remains after the completed R2 probe and its applicable ruling.
