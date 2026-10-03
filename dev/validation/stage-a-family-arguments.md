# Stage A.5b.3.2e.5: expected types for family arguments

Recorded on 2026-10-03 in a separate checkout. That checkout holds the staged
tree of this change on base `ec6354ef7e01144160322e7c1c87b8993f39ca68`. The
execution record gives the path of the checkout.

Family parameters and indices now receive their expected types during public
elaboration. For example, the checker accepts `Ib2 (box 1)` when the index
telescope declares `b : Box Nat`. The constructor gets `Box Nat` as its expected
type and infers its erased parameter `Nat`.

`surface/elab.bend:elab_fam_ref` concatenates the family parameter and index
telescopes and calls the existing `surface/constructor.bend:arguments` walker.
The walker evaluates each declared argument type in the environment of earlier
argument values. It keeps that environment across the parameter/index boundary.
Supplied expressions still elaborate in the caller's context, so open local
variables remain in scope. The existing family arity check runs first.
The kernel declaration check validates the resulting type and term.
When a lambda has an expected function type, the elaborator now requires its
binder annotation to be convertible with the expected domain. When a
constructor gets a type that is not its family, the refusal now also prints
that type.

The constructor suite adds 16 accepted cases and ten refusals. It converts
the `family-argument` refusal into an accepted indexed-family case. This closes
the pending `family-argument` item of
[constructor parameter validation](stage-a-constructor-parameters.md).
Its new cases cover parameters, indices, dependencies within each telescope and
across their boundary, aliases, nested and nullary constructors, and open erased
variables. They also cover family arguments in field types, a mutual sibling, a
mutual-family value, a self reference with a constructor index, an `elim`
return motive and a lambda annotation that agrees through an alias. One case
checks and erases `examples/family-arguments.sole-comb`. The refusals cover
invalid fields, another family and missing or excess arguments. They also
cover a numeral for a `Box`, a wrong index value, a universe without
cumulativity, a constructor for a non-family type and two lambda annotations
that disagree with the expected domain. `examples/family-arguments.sole-comb`
adds four public definitions, and the independent public example census
increases from 91 to 92.

## Recorded checks

| Check | Result |
|---|---|
| Pinned JavaScript host build | PASS; SHA-256 `6c0160651d3d84c8a2fab9de24543d1e4dff66ff91b3aabf9a44c7deca3b853d` |
| Constructor suite on Bun and Node worker | PASS: 56 cases, 35 accepted and 21 refusals; 182 command observations |
| Checked and erased constructor goldens | PASS: 35 pairs; exact output agrees across hosts |
| Constructor mutations | PASS: all six compile and are caught by a semantic refusal |
| Long list regression | PASS: 300 cells; all four observations remain below the 60-second limit |
| Public CLI on Bun and Node worker | PASS: 92 files, two lexical boundary cases, 195 command observations |
| Public recursive families on Bun and Node worker | PASS: exact erased output, 16 literal goldens and the order probe |
| House tests | PASS: 15 tests |
| Bend source policy | PASS: 53 files, 42 line budgets, 60 reviewed unsafe sites, 35 catchalls |
| Reference fixture tests | PASS: 11 tests |
| Native host and full `make gates` | PENDING: not run on this tree |

The two new mutations remove expected types from family arguments or put indices
before parameters in the telescope. The first refuses the unannotated `box`
index. The second refuses `family-parameter-index-order`, whose index type
reads a parameter, with an unbound de Bruijn index. The four existing constructor mutations also
compile and fail their intended cases. A compilation failure does not count
as a semantic mutation result.

The [constructor record](stage-a-family-arguments.json),
[public CLI record](stage-a-family-arguments-cli.json) and
[public family record](stage-a-family-arguments-family.json) contain source
hashes and exact observation hashes. The
[execution record](stage-a-family-arguments-execution.json) gives the command,
start and end times, duration and exit code of each step. It lists every
attempt, including the failed attempts of the first recording. It also gives
the native/full-gate status. Adjacent build, constructor, CLI, family and checks
logs retain their output. All recorded source hashes were verified before these
artifacts were copied into the repository.

## Native status

Native compilation and the full `make gates` did not run on this tree. The
recording machine had low free disk space. The JavaScript checks used cached
host artifacts. Native validation of A.5b.3.2e.4 also stays pending. Signal 9
stopped the recorded native build of that increment.
[Constructor parameter validation](stage-a-constructor-parameters.md#native-status)
records that attempt.

## Reproduce

Run these commands from the repository root to repeat the recorded checks:

```sh
python3 -P dev/build.py --backend js
python3 -P dev/test-constructor-parameters.py --hosts bun,node-worker --mutations
python3 -P dev/test-cli.py --hosts bun,node-worker
python3 -P dev/test-public-family.py --hosts bun,node-worker
python3 -P dev/test-house.py
python3 -P dev/house-bend.py
python3 -P dev/test-reference-fixtures.py
```

Run these commands to complete the remaining native and full gates:

```sh
python3 -P dev/build.py --backend native
make gates
```

Recursive definitions, structural recursor sugar, unconstrained
parameterized-constructor inference, family default arms, complete erased-corpus
integration, WebAssembly build/run and the full kernel differential remain
pending.
