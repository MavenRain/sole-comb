# Stage A.5b.3.2e.2: recursive-family constructor and match erasure

This increment ports `mu_intro`, `mu_tag`, `mu_fields`, `mu_elim`, `mu_case`,
`mu_branch_of` and `mu_binders` from Kanon revision
`69f3be5198cda4334de4fd23b4ccc92cac595789`, `lib/erase.ml:942-1134`.
`erase/mu.bend` prepares typed constructor fields and branch binders and
computes dependent branch targets using the kernel's family rules.
`erase/core.bend` resumes those plans through its fuel-bounded term requests.

Constructors become `KTag` and matches become `KCase`. Both use declaration
order for tags. Type, proof and zero-quantity fields disappear when the generic
layout omits them. A retained generic slot still produces `KErased` if its
actual type erases, preserving the positions of later fields and binders.
Branch binders keep the constructor's quantities and generic layout, while
their actual types and result indices are evaluated in the growing field
environment. Branch bodies inherit the tail flag. Constructor groups include
every completed constructor and survive nested erasure and closure lifting.

The public grammar still refuses recursive-family syntax. The test frontend
already admits families, so `test/family-erasure.bend` checks those programs
and invokes the production erasure pass. Recursive-definition erasure,
public family syntax, complete erased-corpus integration, WebAssembly build/run
and Stage A acceptance remain pending.

Commands:

```text
make test-family-erasure
make test-family-erasure-mutations
make house test-reference-fixtures test-core-erasure-mutations test-family-layout-mutations test-cli build
```

With the tracked 900-second limit, the first family and regression runs timed out in native compilation. They passed with the local retry script in `stage-a-family-erasure-execution.json`. The review rerun passed the family and regression checks with the tracked limits.

`dev/test-family-erasure.py` checks six fixtures and 28 independent literal
goldens. Each golden is checked only against the output of its own fixture:
17 for `test/family-erasure.kan`, 7 for `test/family-erasure-runtime.kan` and
4 for `mu-dependent-layout.kan`. The new `test/family-erasure.kan` covers
nullary and nested constructors, reordered branches, erased proof and zero
fields, outer captures, indexed targets, mutual families and empty families.
The new `test/family-erasure-runtime.kan` declares the families `N`, `Ghost`,
`RP` and `Two`. `RP` has a runtime parameter. Its definitions `rpRead`, `mkN`,
`scrutApp`, `nonTail`, `tagClosure` and `firstOf` cover a match on `RP 3`, a
scrutinee that is an application, a match that is not in tail position, a
lifted closure that contains a family tag and match, and a match with two kept
fields.
Two existing pinned fixtures, `mu-dependent-layout.kan` and
`mu-parameter-layout.kan`, reach `erase/mu.bend` and cover dependent generic
slots and parameterized constructors. The other two, `mu-indexed.kan` and
`mu-empty-large-elim.kan`, erase only to `erased` declarations. They do not
reach `erase/mu.bend`; they check only that the pass keeps those outputs. The
comparisons include the whole exact erased output on Bun, Node worker and
native, including groups and lifted functions.

Ten direct contracts in `test/core-erasure.bend` cover wrong former and
address, missing family and branch, and field/binder arity errors. Four of
them are new: `family-intro-short-args`, `family-intro-short-layout`,
`family-branch-short-names` and `family-branch-short-layout`. They reach the
short argument, short name and short layout arms. `erase/mu.bend` now has two
error texts from Kanon `lib/erase.ml:1034-1037`: "an introduction stands at a
type that is not a left former" and "an elimination stands on a value that is
not a left former". One helper, `Family.not_ctor` in `erase/family.bend`,
supplies the not-a-constructor text.

No test covers a value of a family with parameters in the parameter
environment of `erase/mu.bend`. Kanon `surface/elab.ml:440-449`
(`elab_ctor_ref`) refuses every constructor of a family with parameters
("needs an expected type"). Thus no Kanon text builds such a value, and the
review dropped the planned mutant that replaces that environment with `[]`.
The closest covered form is `rpRead`, a match on `RP 3` with a runtime
parameter.

The new reference fixture is a one-time capture from an existing Kanon CLI
binary, `kanon.exe check --erased PATH`, on 2026-10-02. The checkout was
clean at the pinned revision. Its binary sha256 is
`d8b522e8b92ce11227fb16c5420d472e33f601013721e1826e9d04fc3231ccc3`.
The fixture preserves exact stdout; every capture exited 0 with empty stderr.
The binary's source build revision was initially unverified. A fresh build in
an isolated clone of the clean pinned source produced the same binary sha256
and the same exact outputs for the first five fixtures. The review added
`test/family-erasure-runtime.kan` and captured it with the same binary; it
exited 0 with empty stderr. The build command, toolchain versions, source tree
and checks are recorded in `stage-a-family-erasure-reference.json`. The original Kanon
checkout remained read only. This was a one-time provenance check.
No adapter or OCaml source is added. Routine tests load the frozen fixture
through `dev/reference-fixtures.py`, which checks the revision, fixture hash
and input hashes. Existing reference fixtures are unchanged.

Eight isolated mutations target constructor tags, ghost-field retention,
runtime binder order, branch tags, constructor-group accumulation, the groups
of lifted closures (`erase/core.bend:293`), the scrutinee tail flag and the
branch tail flag. The lifted-groups and branch-tail mutants run on the runtime
fixture. The binder-order mutant appends instead of prepends in `bound_cons`
(`erase/mu.bend:114`) and runs on `mu-dependent-layout.kan`. There, `last`
changes from `{0 2 (KVar 0)}` to `{0 2 (KErased)}`, because its kept and
erased binders are not symmetric. On the runtime fixture this mutant gives the
same output: both fields of `firstOf` are kept, so a reversed list gives the
same `KVar` index. Mutants must compile successfully and then differ in
the semantic observation on Bun. A mutant whose Bun output starts with `FAIL`
or whose stderr is not empty does not count as a kill.
The new `@unsafe` registry entry is only `erase/core.bend:mu_branches`, whose
recursion consumes the finite constructor list and whose term requests spend
erasure fuel. All type, affine-use and exhaustive checks stay enabled. The
core file limit increases from 660 to 730 lines, and the new planner has a
180-line limit; the other source limits and policies stay in force.

The JSON record captures exact host and mutation observations, the reference
provenance and all transitive test-driver source hashes. It refuses source
changes during validation. Full logs are retained alongside this note. The
harness writes its result to `_build/family-erasure/result.json` and never
writes the tracked record. The close step copies the postfix result into
`stage-a-family-erasure.json`.

The first revision completed validation on 2026-10-02, before the review. The
family harness passed all five exact
fixture comparisons and fourteen literal goldens on Bun, Node worker and
native, and killed all five semantic mutants. The regression run passed eight
core-erasure oracle comparisons on each host, eighteen core mutants, six
family-layout fixtures with eleven goldens and four mutants, and the public
CLI suite (217 observations and 68 files per host). The JavaScript build step reused its cached output, which is keyed by the transitive source hashes. House policy, fifteen house tests and eleven reference-integrity tests also passed. The refreshed core and family-layout
JSON records preserve their
earlier milestone scopes as regression evidence.

The review rerun on 2026-10-02 used isolated copies of the staged tree and the
tracked time limits. Family erasure passed six fixtures and 28 literal goldens
on Bun, Node worker and native, and killed all eight semantic mutants, in 267
seconds. Before the review fixes, the same command passed the five earlier
fixtures in 388 seconds with the same limits. The regression run passed eight
core-erasure oracle comparisons, three pair goldens and one refusal on each
host, and eighteen core mutants. Family layout passed six fixtures, 21
families and eleven goldens on each host, and four mutants. Its first attempt
stopped when an external SIGKILL ended the native build at load 43; the retry
passed. The public CLI suite passed 217 command observations and 68 native
files per host. Elaboration passed 221 cases on each host and three mutations.
The program suite passed 245 files in two modes on each host and three
mutations. The build check, the full pin check (34 checks), house policy,
fifteen house tests, the reference-integrity tests, and the foundation,
representation, evaluation, checking, pinfront and erasure suites with their
mutations also passed. The family erasure and family layout results come from
the final copy, which includes the binder-order mutant. The other refreshed
records come from a copy without it; that change touches no file that they pin.
In those records only source pins, derived evidence hashes and run paths
changed.

The first standard runs timed out during native compilation at the unchanged
900-second harness limit. Retrying the same semantic checks with a local
2400-second native compilation and public-command deadline passed. The local
toolchain override also set `build_deadline_s` to 2400; the tracked 900-second
harness and 1200-second build limits remain unchanged. These retry results
established semantic agreement only. The review rerun then passed with the
tracked limits on one local machine. Full Stage A acceptance remains pending.

The successful run logs are `stage-a-family-erasure-hosts.stdout.log` and
`stage-a-family-erasure-regressions.stdout.log`. The initial timeout logs use
the `default-family` and `default-regressions` suffixes, with both stdout and
stderr preserved. The `policy` logs preserve the final policy and integrity
checks. `stage-a-family-erasure-execution.json` records the local
override and exact invocation context. The staged sources are checked against
all three source-hash records before publication.

The first validation ran in a separate local worktree whose tracked sources had the staged hashes. The logs contain its absolute paths. The review rerun used an isolated copy of the staged tree in a temporary directory, so the refreshed core-erasure observations contain the paths of that copy. The execution record describes the first runs and stores the retry script in full; to run it again, write it to a file and set `root` to the checkout.

`no-ocaml-cleanup.json` stays as a point-in-time record. Its hashes refer to
the earlier core-erasure and family-layout records, which this increment
replaced.
