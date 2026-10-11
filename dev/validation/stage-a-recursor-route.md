# A.5b.3.2e.18: kernel Elim routing to checked recursors

The kernel now sends a mu-shaped `Elim` to the checked recursor when a
recognized constructor leg binds more binders than that constructor has
fields and the constructor has a recursive field. Both inference and
checking use this dispatch. Other eliminations retain the kernel case
analysis rule. The result closes the declaration parameter aliases out of
the branch usage, and evaluation refuses delayed recursive branches.
Public hypothesis syntax, recursive iota, erasure, empty-family recursors,
mutual and affine routed families, and Stage A acceptance remain pending.

The companion [saved validation ledger](stage-a-recursor-route.json)
contains the available phase summaries, their hashes, the saved Part 1
ledger, and a separately dated source snapshot. It is a log-backed record,
not a regenerated per-case harness result. The final native attempt removed
the harness result file before exceeding the memory cap. Per-case native
observations and final per-mutant kill observations are not reconstructed.

## The three slices

- **2e.18a, USER commit `7109d78`:** layout, uniform parameter, instance and
  group helpers moved to generic kernel programs with context operations
  supplied by the runtime. The bridge preserved concrete callers and the
  constructor context-local mutation. The commit reports a native
  representation compilation timeout. Its original per-phase summaries
  are unavailable in the retained kit, so no numerical gate claims are
  reconstructed for this slice.
- **2e.18b, USER commit `52e654b`:** motive, branch, body and coverage checks
  moved to generic kernel programs. Coverage is interpreted at the
  concrete elimination boundary. The USER preserved the generic ambient
  context-shift mutations and the motive parameter-environment mutation.
  This slice is **validated by the USER**, as recorded in the USER's
  commit, with no claim of a fresh rerun of that commit here.
- **2e.18c Part 1:** the elimination entry moved to generic programs,
  `kernel_check.rules` became the concrete interpreter, and
  `kernel_recursor_bridge.bend` was deleted. The adapters and scope
  mutations were re-anchored without dropping coverage. The saved index
  checkpoint is `75ca6a76b9ce60d0717c2739d237d6fcdb2ded92`.
- **2e.18c Part 2:** kernel dispatch, parameter-row closing, the evaluation
  guard and the route harness were added. The route Make targets cover Bun
  and Node worker, a Bun mutation run, and a separate explicit native run.

## Contract and rulings

`kernel_recursor_route.routes` is a pure predicate. It recognizes an actor
address, finds that constructor in the mu family, and requires both excess
binders and `P.Ctor.c_self_rec`. The latter is computed by
`Pos.self_rec(group, args)`: a field type mentions a family name in the
declaration group, including a mutual sibling. It is not proof that the
routed recursor supports mutual families. At either kernel `T.Elim` site,
a false predicate calls `R.elim_elim` with the existing term and expected
type. A true predicate requires the motive and calls `Elim.check`.

The USER selected ruling **(b)**: excess binders alone do not route a
nonrecursive constructor. The pinned A.4 checking oracle is unchanged.
`routed-nonrecursive-extra` became `kernel-nonrecursive-extra` with the
kernel arity refusal, and `routes-nonrecursive` became the final added
mutation. Mixed legs still route when the recursive leg meets the
predicate. No pinned expectation changed; only the nonrecursive route
row moved from the recursor refusal group to the kernel refusal group.

`Elim.result` calls `R.c_close` at the given-context boundary before
sequencing scrutinee usage with scaled branch usage. Parameters are
`Q.Zero` aliases. Closing removes their rows without adding a refusal.
The existing instance, elimination and ambient printed expectations did
not change: their printed names are given locals, and parameter rows have
zero reads and are not printed by `Q.used`.

`beta_body` requires the selected leg's binder count to equal the
constructor argument count before evaluating the body. A mismatch returns
`E.Not_yet` with the delayed recursive evaluation refusal. This guards the
pending recursive evaluation operation; it does not implement recursive
iota.

The initial **kernel-empty harness row was wrong**. Kernel case analysis
at an empty constructor list produces `Q.unreachable`. Sequencing this
with the scrutinee preserves its reads but leaves unreachable paths, so
the driver's interval is `e:1..w`. The row was corrected to that result;
the kernel implementation and the pinned oracle were left unchanged.

## Saved validation

The numeric results below come from the retained phase summaries or the
saved Part 1 ledger at line 34 of `work/progress-18c.md`. Older compile
failures and the initial route mismatch are retained in the JSON ledger
with their actual exit codes. They are superseded by the named passing
runs, not relabeled as passes.

Part 1 completed the adapter compiles and recursor regression runs. Its
saved ledger records evaluation and program passes, a complete pinfront
run (`cs1`, 3600 MB), CLI (`cr5e`) and policy/build/pins (`cr6`). The native
core-erasure, representation and checking legs exceeded the cap in that
part and remain unclaimed. The native pinfront diagnostic used the same
4000 MB cap with a closer process sampler. Compiler heap variance explains
the spikes in the saved diagnosis; it does not turn a killed run into a
validation result.

The Part 2 helper compiles all passed in `cd7` and `cd8`. The route driver
compiled in `cd10`, and its evaluation command compiled in `cd12`.
The following regression runs precede ruling (b):

| Suite | Cases on Bun and Node worker | Bun mutations | Evidence |
| --- | ---: | --- | --- |
| instance | 53 | 11 killed | cr7, cr8 |
| elimination | 43 | 6 killed | cr7, cr8 |
| ambient | 69 | 6 killed | cr7, cr8 |
| layout | 19 | PASS, count not printed | cr11 |
| uniform | 34 | PASS, count not printed | cr11 |
| motive | 43 | 8 killed | cr11 |
| branch | 44 | 9 killed | cr11 |
| body | 46 | 8 killed | cr11 |
| coverage | 45 | 9 killed | cr11 |

The final route run after ruling (b) is `cr13`: **41 cases on Bun and Node
worker and 10 killed Bun mutations**. `cr14c` repeats the 41 Bun cases
after the comment-only line-cap fix. The nine earlier mutation witnesses
were inspected in the saved handoff; the final ten-mutation run prints an
aggregate pass. Its individual kill text was not retained as a result
record and is not claimed here. The current case and mutation names are
listed in the JSON ledger.

`cr11` evaluation passed 251 cases, and program passed 245 files in both
modes. `cr11b` representation passed 465 cases. `cr12` reran checking after
ruling (b), with a whole-run pass for 291 cases and the harness's native
INFO observation. `cr14b` build and pins passed; its policy failure was the
route module exceeding the 32-line house budget. Folding the header
comment restored the budget, and `cr14c` policy passed.

| Incomplete Part 2 gate | Peak MB | Recorded scope |
| --- | ---: | --- |
| cr11 core-erasure | 4066 | Bun and Node worker pass lines only; native unclaimed |
| cr11b pinfront | 4308 | Pinned parser fixture pass line only; native unclaimed |
| cr11c CLI | 4212 | Bun and Node worker pass lines only; native and compiler legs unclaimed |
| cr14 CLI | 4425 | Bun and Node worker pass lines only; native and compiler legs unclaimed |
| cr15 route native | 4081 | CAP-KILL before a route case line; NOT VALIDATED |

The CLI native leg rebuilds the public native executable when sources
change. The saved executable was dated October 6 at 21:28. Part 1's CLI
run reused it; the Part 2 CLI attempts exceeded the cap during its rebuild.
The memory cap remains 4000 MB. `cr15` is the final native route attempt;
it was not retried, and native route validation remains open.

`stage-a-program.json` is kept and staged under the USER's earlier ruling.
Relative to the Part 1 index, only its source hashes change; its counts and
observations agree. The added route helper is included in that recorded
source closure. This historical program record is not rewritten to claim
validation of later source edits.

## Coverage limits and remaining work

The printed usage observations cannot kill `close-dropped` or
`close-at-origin-size`: leaked zero-quantity parameter rows never print.
They cannot kill `motive-none-ignored` because the compared paths print
the same `Cannot_infer` text. These candidates are recorded as not
killable by this harness and are excluded from its mutation count. This
does not establish that the corresponding implementation changes are
semantically harmless.

The driver did not supply fixtures for a mutual routed family, an
erased-mode type-valued parameter-slot result, two declaration parameters
at distinct given types in reverse order, or a closed-context linear read
in both scrutinee and body. Its mixed-leg witness uses the zero constructor
with an extra binder rather than two recursive constructors. Public
hypothesis syntax, recursive iota, recursor erasure, empty-family
recursors, mutual and affine routed families, nondirect recursion, nested
`Rec.rules` budgeting and the earlier universe observation remain open.
The incomplete native gates above are also pending.
