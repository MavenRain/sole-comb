# Stage A.5b.3.2e.4: constructor parameters from expected types

Recorded on 2026-10-03 in a copy of the staged tree of the checkout
`/Users/oobi/Documents/sole-comb`. Each tracked file in the copy is equal to its
staged blob. The execution record gives the path of the copy. The base is
`1815c8dbd2d78af9134192600b5a3a4fe096a4f7`. The review fixes are in this tree.

The public elaborator accepts `def value : Box Nat := box 7` for
`mu Box (0 A : Type 0) : Type 0 { box : A -> Box A }`. It gets the
parameters from the expected family type. Then it elaborates each field
against its instantiated telescope. Expected types reach constructor
applications and nullary names.

`surface/constructor.bend` contains the parameter decoder and the
dependent-field walker. Earlier field values extend the telescope environment.
The walker evaluates a field value only when another field follows it. The
elaborator evaluates the index values only when the family has indices. The
walker does not call the kernel for each field or for the completed term.
The declaration check validates the parameters, fields, indices and usage once.
Constructors without parameters use the same field walker, so their fields also
get expected types. When a constructor without parameters has an expected
type, the elaborator also checks the family name. Kernel rules, conversion,
positivity and erasure semantics do not change.

A refusal for another family names the constructor, its family and the
expected family. For example, `other 7` against `Box Nat` gives
`the constructor other of Other cannot have the expected family Box`.
A constructor without an expected type gives
`the constructor box needs an expected type`.
`examples/constructor-parameters.sole-comb` adds the definitions `wrapped` and
`numbers`, and now has 17 definitions.

## Recorded checks

| Check | Result |
|---|---|
| Pinned JavaScript build | PASS in 0 s (cached); artifact SHA-256 `e3bf1950737dfc9d9bed1df0a0aab2f92dccb801ab5b1694b75890d50c16e012` |
| Constructor suite, Bun and Node worker | PASS: 30 cases (18 accepted, 12 refusals), 96 command observations, checked and erased output equal across hosts |
| Constructor goldens | PASS: 18 checked goldens and 18 erased goldens in the `--print` and `--erased` output |
| Constructor semantic mutations | PASS: all 4 mutations compile and are killed |
| Public CLI, Bun and Node worker | PASS: 91 native files and 2 lexical boundary cases, 193 command observations; source hashes stable |
| Public recursive families, Bun and Node worker | PASS: exact erased output, 16 literal goldens, exact order probe |
| House tests | PASS: `OK` |
| Bend source policy | PASS: `HOUSE PASS scoped host policy: 53 files, 42 line budgets, 60 reviewed unsafe sites, 35 catchalls` |
| Reference fixtures | PASS: `OK` |
| Native build and full `make gates` | PENDING on this tree |

The build step found the artifact current for these sources and reused
it. An earlier compile of the same sources gave the same SHA-256.

The Bend source policy counts 35 catchalls. The new catchall is
`owner_shape` in `surface/constructor.bend`, and `dev/bend-policy.json`
records it.

The [execution record](stage-a-constructor-parameters-execution.json) gives each
command with its start time, end time, duration and exit code. The
[constructor result](stage-a-constructor-parameters.json), the
[CLI result](stage-a-constructor-parameters-cli.json) and the
[family result](stage-a-constructor-parameters-family.json) pin the SHA-256 of
each validation source. The adjacent build, constructor, CLI and family logs
keep the reported results.

## Step durations

The steps ran one at a time, in this order.

| Step | Command | Start (UTC) | End (UTC) | Seconds | Exit |
|---|---|---|---|---|---|
| `js-build` | `python3 -P dev/build.py --backend js` | 2026-10-03T08:50:13Z | 2026-10-03T08:50:13Z | 0 | 0 |
| `constructor` | `python3 -P dev/test-constructor-parameters.py --hosts bun,node-worker --mutations` | 2026-10-03T08:50:13Z | 2026-10-03T08:52:15Z | 122 | 0 |
| `cli` | `python3 -P dev/test-cli.py --hosts bun,node-worker` | 2026-10-03T08:52:15Z | 2026-10-03T08:57:19Z | 304 | 0 |
| `family` | `python3 -P dev/test-public-family.py --hosts bun,node-worker` | 2026-10-03T08:57:19Z | 2026-10-03T08:57:23Z | 4 | 0 |
| `test-house` | `python3 -P dev/test-house.py` | 2026-10-03T08:57:23Z | 2026-10-03T08:57:23Z | 0 | 0 |
| `house-bend` | `python3 -P dev/house-bend.py` | 2026-10-03T08:57:23Z | 2026-10-03T08:57:24Z | 1 | 0 |
| `reference-fixtures` | `python3 -P dev/test-reference-fixtures.py` | 2026-10-03T08:57:24Z | 2026-10-03T08:57:24Z | 0 | 0 |

## Cases and goldens

The suite has 18 accepted cases and 12 refusals. The accepted cases cover
annotations, nested constructors, open parameters, type aliases, function
arguments and nullary constructors. They also cover parameter order, dependent
fields, closed and open indices, local-name shadowing and erased arguments.
Lists, mutual families, annotated scrutinees and parameterless constructors
with parameterized fields complete the set. The refusals cover field types and
arity, family and index mismatches, and dependent-field and parameter order.
They also cover unconstrained inference, family arguments without an
annotation, runtime use of erased variables, and the current UAT equality and
record blockers.

Each accepted case has a definition count and two literal goldens. The checked
golden is a substring of the checked `--print` output. It holds the elaborated
constructor term and its fields. The erased golden is a substring of the
`--erased` output. The suite runs both modes on each host.

| Case | Definitions | Checked golden | Erased golden |
|---|---|---|---|
| `alias` | 2 | `def value : Alias := (In SMu Box [] (ACtor box) [7])` | `fun value () : union mu<Box> := KTag mu<Box> 0 [KLit 7]` |
| `annotated-scrutinee` | 1 | `((In SMu Box [] (ACtor box) [7]) : (Lan SMu Box [] (Sec SColl 1 [ => Nat])))` | `KCase mu<Box> (KTag mu<Box> 0 [KLit 7]) [{0 1 (KVar 0)}]` |
| `annotation` | 1 | `def value : (Lan SMu Box [] (Sec SColl 1 [ => Nat])) := (In SMu Box [] (ACtor box) [7])` | `fun value () : union mu<Box> := KTag mu<Box> 0 [KLit 7]` |
| `argument` | 2 | `(APt w (In SMu Box [] (ACtor box) [7]))` | `fun value () : union nat := KTail (KGlobal read) [KTag mu<Box> 0 [KLit 7]]` |
| `dependent-fields` | 1 | `(In SMu DepBox [] (ACtor depBox) [(Ran SColl 0 (Sec SColl 0 [])); (Sec SColl 0 []); 7])` | `fun value () : union mu<DepBox> := KTag mu<DepBox> 0 [KErased; KLit 7]` |
| `erased-argument` | 2 | `(APt 0 (In SMu Box [] (ACtor box) [n]))` | `fun value () : union nat := KTail (KGlobal ignore) []` |
| `generic` | 1 | `[a => (In SMu Box [] (ACtor box) [a])]` | `fun value (union any) : union mu<Box> := KTag mu<Box> 0 [KVar 0]` |
| `indexed` | 1 | `(In SMu At [7] (ACtor at) [])` | `fun value () : union mu<At> := KTag mu<At> 0 []` |
| `list` | 1 | `(In SMu List [] (ACtor cons) [1; (In SMu List [] (ACtor cons) [2; (In SMu List [] (ACtor nil) [])])])` | `fun value () : union mu<List> := KTag mu<List> 1 [KLit 1; KTag mu<List> 1 [KLit 2; KTag mu<List> 0 []]]` |
| `long-list` | 1 | `(In SMu List [] (ACtor cons) [1; (In SMu List [] (ACtor nil) [])])` | `KTag mu<List> 1 [KLit 1; KTag mu<List> 0 []]` |
| `mutual` | 1 | `(In SMu Ev [] (ACtor econs) [1; (In SMu Od [] (ACtor ocons) [2; (In SMu Ev [] (ACtor enil) [])])])` | `fun value () : union mu<Ev> := KTag mu<Ev> 1 [KLit 1; KTag mu<Od> 0 [KLit 2; KTag mu<Ev> 0 []]]` |
| `nested` | 1 | `(In SMu Box [] (ACtor box) [(In SMu Box [] (ACtor box) [7])])` | `fun value () : union mu<Box> := KTag mu<Box> 0 [KTag mu<Box> 0 [KLit 7]]` |
| `nullary` | 1 | `(In SMu Maybe [] (ACtor none) [])` | `fun value () : union mu<Maybe> := KTag mu<Maybe> 0 []` |
| `open-index` | 1 | `[x => (In SMu At [x] (ACtor at) [])]` | `fun value () : union mu<At> := KTag mu<At> 0 []` |
| `parameter-order` | 1 | `(In SMu Both [] (ACtor both) [(In SMu Box [] (ACtor box) [7]); (Sec SColl 0 [])])` | `fun value () : union mu<Both> := KTag mu<Both> 0 [KTag mu<Box> 0 [KLit 7]; KErased]` |
| `plain-dependent` | 1 | `(In SMu S [] (ACtor s) [(Lan SMu Box [] (Sec SColl 1 [ => Nat])); (In SMu Box [] (ACtor box) [7])])` | `fun value () : union mu<S> := KTag mu<S> 0 [KTag mu<Box> 0 [KLit 7]]` |
| `plain-wrapper` | 1 | `(In SMu W [] (ACtor w) [(In SMu Box [] (ACtor box) [7])])` | `fun value () : union mu<W> := KTag mu<W> 0 [KTag mu<Box> 0 [KLit 7]]` |
| `shadowed` | 1 | `[box => (Out SPi w _ Nat (APt w 7) box)]` | `fun value (func fn<1>) : union mu<Box> := KTail (KVar 0) [KLit 7]` |

Family arguments do not get expected types yet. For
`mu Ib2 : (0 b : Box Nat) -> Type 0 { ib2 : (0 b : Box Nat) -> Ib2 b }`,
the type `Ib2 (box 1)` is refused with
`cannot infer: the constructor box needs an expected type`. The case
`family-argument` records this refusal. A review probe showed that the type
`Ib2 (box 1 : Box Nat)` is accepted. Expected types for family arguments
remain pending.

## Mutations

| Mutant | File | Change | Case | Kill marker | Refusal | Compile exit | Exit |
|---|---|---|---|---|---|---|---|
| `expected-type` | `surface/elab.bend` | routes the constructor around the expected type | `annotation` | `the constructor box needs an expected type` | `cannot infer: the constructor box needs an expected type` | 0 | 1 |
| `parameter-order` | `surface/constructor.bend` | drops the reversal of the parameter environment | `parameter-order` | `the constructor box of Box needs the family Box as its expected type` | `mismatch: the constructor box of Box needs the family Box as its expected type` | 0 | 1 |
| `dependent-environment` | `surface/constructor.bend` | drops earlier field values from the telescope environment | `dependent-fields` | `unbound: de Bruijn index 2 is outside the environment` | `unbound: de Bruijn index 2 is outside the environment` | 0 | 1 |
| `plain-fields` | `surface/elab.bend` | elaborates parameterless fields without expected types | `plain-wrapper` | `the constructor box needs an expected type` | `cannot infer: the constructor box needs an expected type` | 0 | 1 |

Each mutant must compile before its check. A compile failure does not count as
a kill. A kill needs exit 1, a `FAIL` line on stderr and the kill marker. The
refusal column gives the first stderr line after `FAIL`.

## Timing control

The earlier code checked each field and the completed constructor with the
kernel. The cost grew quickly with the length of a list literal. Earlier review
probes on Bun measured 36 s for a 160-cell list. A 320-cell list went over the
120 s host timeout. A control without parameters checked 640 cells in 8.9 s.

On this tree the 300-cell `long-list` case checks in 0.696 s and erases in 1.754
s on Bun. On Node worker it checks in 1.365 s and erases in 1.371 s. The suite
fails a run at 60 s or more. These results are for one list size only. They do
not show a growth rate.

## Native status

An earlier native build ran in the checkout
`/Users/oobi/Documents/gpt5/sole-comb-a5b3-e4-1815c8d`, before the review fixes.
All terms checked. Then the native compiler was killed by signal 9 (`exit=-9`).
The cause is not recorded. The `-native` stdout and stderr logs keep that
attempt. Native validation and the full `make gates` stay pending on this tree.
The main loop runs native validation next.

The `family-parameter-ctor` refusal fixture puts its constructor in an
unannotated elimination scrutinee. Its original annotated construction is
supported, and the positive cases cover it. The negative manifest and its
inference diagnostic stay required.

## Reproduction and remaining gates

```sh
python3 -P dev/build.py --backend js
python3 -P dev/test-constructor-parameters.py --hosts bun,node-worker --mutations
python3 -P dev/test-cli.py --hosts bun,node-worker
python3 -P dev/test-public-family.py --hosts bun,node-worker
python3 -P dev/test-house.py
python3 -P dev/house-bend.py
python3 -P dev/test-reference-fixtures.py
```

The constructor targets default to Bun, Node worker and native. `make check`
includes the ordinary suite. `make test` and `make gates` include the mutation
suite. These primary-host results do not establish a full `make gates`,
A.close, KANON-DIFF or R2 acceptance result.

This increment advances Stage A preparation for UAT. Some items remain pending.
They are constructor inference without an expected family, expected types for
family arguments, structural recursor sugar, and equality and transport in
`Prop`. Dependent records, universe polymorphism and instance resolution also
remain pending.
