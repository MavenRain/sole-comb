# Public closed records

This increment implements `record Name : Type i { field : T; ... }` in the
public compiler. A record expands to a Ran product alias and one qualified
Out getter for each field. Construct values with `tuple(...)`, and apply a
getter as `Name.field value`. Labels occupy the record's qualified namespace.

`surface/record.bend` parses and expands declarations before the ordinary
production elaborator and kernel checker. It preserves field order, rejects
duplicate labels, and generates a binder that cannot occur in source names.
Both parser loops spend finite token-count fuel. Their two reviewed `@unsafe`
sites retain Bend's type, affine-use, and exhaustiveness checks. The existing
parser remains within its 900-line limit; the new module has a 150-line limit.
The production import closure continues to exclude `test/pinfront/`.

Field types use preceding globals. Empty, nested, heterogeneous, function,
and higher-universe fields are supported. These types retain structural
product equality. Dependent fields, record parameters, and recursive records
remain outside this prefix. Full type-directed erasure, build/run, complete
KANON-DIFF, and Stage A acceptance remain pending.

## Public expectations

The suite contains 53 native files: 10 accepted examples and 43 refusals.
The record increment adds three examples and 15 refusals. It checks the
public command's actual stdout, stderr, and exit status on each host.

`records.sole-comb` checks 32 definitions, including field indices zero through
three, different namespaces with the same label, empty and nested products,
function-valued fields, universe admission, generated-name hygiene, and a
qualified getter used as an elimination arm, and a single linear projection.
Each getter takes its record with quantity one, so a caller can consume a
linear record once. The duplication refusal checks that two projections
still fail. `records.kernel` preserves the
reviewed product shapes, field types, indices, and binder expectations.

`record-expansion.sole-comb` and its independently written explicit expansion
each check seven definitions. The public suite compares their checked kernel
output after renaming only the generated `#record` binder to the explicit
example's `recordArg`. Existing default-arm equivalence checks remain exact.
All public examples must also print identically across hosts.

The new refusals cover duplicate labels, missing colons or field types,
missing braces, incorrect separators, non-type fields and annotations,
universe mismatches, recursive and dependent field references, unknown fields,
invalid qualified projection targets, incorrect tuple fields, and duplicated
linear record values. The explicit example census and source-hash stability
checks remain required.

## Validation

Validation runs in an isolated local copy of sole-comb at base `896e914`.
The focused Bun suite passed 61 public command observations after the linear
getter fix. The complete `make gates` run exited zero. It passed A.1 through
A.5b.3.1 comparisons and their mutation checks, all 172 public command
observations on Bun, Node worker, and native, 35 pin checks, and the compiler
dependency-closure check. HOUSE passed 44 files, 33 line budgets, 37 reviewed
unsafe sites, and 29 registered catch-all functions.

The final review covered declaration and field order, fuel consumption,
namespace lookup, binder hygiene, quantity-one getters, universe and type
refusals, source policies, the native census, and the independent expectations.
The successful single linear projection and failed duplication exercise both
sides of the getter quantity rule. No existing line budget or semantic gate
was removed or relaxed.

The checked-in artifacts are `public-records.json`,
`public-records.gates.stdout.log`, and `public-records.gates.stderr.log` in
this directory. The JSON preserves every public command observation and the
source hashes from the final three-host run. Native host checks retain their
INFO status for endpoint qualification; no runtime endpoint was selected.
