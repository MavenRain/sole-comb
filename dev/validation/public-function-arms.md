# Public function-arm validation

On 2026-09-30, `python3 -P dev/test-cli.py` passed 73 public-command
observations on Bun, Node worker, and native. Each host checked five native
examples, rejected fifteen source fixtures, and exercised two lexical
boundary cases. Seven additional observations covered command errors and
file handling. The suite required identical printed definitions across hosts
and verified that all 57 recorded input hashes stayed stable.

`examples/function-arms.sole-comb` checks named functions, partial applications,
local function values, explicit motives, multi-binder lambdas, and linear
functions. Its thirteen declarations include a linear local function reused
across alternative arms, and a nested elim over an outer local inside a
non-lambda arm. New refusals cover nonfunctions, wrong domains, wrong results,
duplication of a linear local function across two eliminations, and a
dependent codomain in a function arm. The public launcher produced every
recorded observation in `public-function-arms.json`.

The parser retains positional arm keys. A lambda arm, multi-binder lambdas
included, takes the binder path: the parser gives `BrLeg{k, [a], fun rest =>
e}`, with the first binder `a` as the payload and any remaining binders
`rest` staying in the branch body `e`. Only a non-lambda arm term goes
through `elab_arm`. Elaboration
checks its domain against the selected sum leg, retains its parameter
quantity, and checks the application result against the expected result or
explicit motive. `elab_arm` elaborates the arm head once, under a hidden
context binder, and runs its domain, quantity, and result checks under the
Pi binder's own name; a nested non-lambda arm no longer pays the cost of
re-elaborating its head at every depth. The internal payload name `$elim`
cannot be a source identifier, and because checks run under the Pi binder
name, diagnostics never show `$elim`. Production changes stay in `surface/`;
the pinned parser and elaborator under `test/pinfront/` are unchanged.

`make house test-bench` passed the source-policy and harness regressions.
`python3 -P dev/build.py --check` passed the public compiler's complete
production dependency closure (6.988 seconds wall time).
An additional `make gates` run passed A.1 through A.5b.1 on all three hosts,
including checking, parser, and elaboration mutation checks. The A.5b.2
oracle completed 245 files in two modes, and Bun began its large agreement
inputs. That broader run was deliberately stopped during comparisons of
the unchanged pinned program path. Its partial stdout and stderr are retained
in `public-function-arms.gates.*.log`; this is not a complete gate pass or
Stage A acceptance claim. The pinned test frontend and original kernel
sources have no changes in this increment.

This increment extends source checking. Type-directed erasure, recursive-family
recursors, records, default arms, and WebAssembly build/run remain pending.

## Review fixes

- A1: the parser gives a single-binder lambda arm `BrLeg{k, [b], e}`
  directly. It no longer wraps the arm body in an empty `SFun` with zero
  remaining binders.
- B1+B2: `elab_arm` elaborates the arm head once, not once per check, so a
  nested non-lambda arm no longer pays a cost that doubled with depth. Its
  domain, quantity, and result checks now run under the Pi binder's own
  name instead of the internal `$elim` name, so diagnostics do not show
  `$elim`. The regression coverage is the `nested` declaration in
  `examples/function-arms.sole-comb` and the new
  `corpus/refuse/function-arm-dependent.sole-comb` fixture.
- D2: `examples/README.md` gained a paragraph describing function-valued
  elim arms.
- D3: this record now states that lambda arms take the binder path and only
  non-lambda arm terms go through `elab_arm`.
