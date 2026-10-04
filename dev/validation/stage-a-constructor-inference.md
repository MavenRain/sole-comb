# Stage A.5b.3.2e.6: constructor inference from field types

Starting point: `8bebcb52cd97bfbec146c0e18c8c0641e6fda14f`.
The kernel remains pinned to Kanon `69f3be5198cda4334de4fd23b4ccc92cac595789`.

An unannotated constructor in an elimination scrutinee now infers its family
parameters from its field types. For example, `elim (box 7) ...` elaborates its
scrutinee to an injection annotated with `Box Nat`. The kernel checks that
annotation, including parameter universes, field types, indices and quantities.
The new surface pass changes no kernel rules or erased constructor layout.

The solver handles direct parameter occurrences and parameters or indices inside
nominal family field types. It reads the fields in declaration order. It
instantiates each field type with the parameters and field values already
determined. Repeated occurrences of a parameter must give convertible values.
The parameters of a nominal field type are also solved in declaration order. As
a result, a conflict refusal gives the value of the earlier position as the
required value. Each unsolved parameter i has the slot level `2^32 + i`. This
level is above every context level, so a slot never aliases a local variable. A
slot quotes to a negative de Bruijn index. The occurs check quotes the field
type and scans every subterm for such an index. Readback uses the caller's
original context after all parameters have been resolved. A refusal in a field
argument names the constructor and the field, for example
`the constructor r needs an expected type or an inferable field: field 1 has no inferable type (...)`.
A parameter without field evidence gives
`the constructor nil needs an expected type or an inferable field for the parameter A`.
Unannotated constructors of parameterless families also retain an annotation so
that the kernel can infer their types in scrutinee positions.

The expected-family path is preserved. Its fields continue to receive their
declared expected types without an additional kernel check for every field.
The public example is `examples/constructor-inference.sole-comb`, with eight
checked definitions. The negative constructor fixture now tests a nullary
parameterized constructor without field evidence. The earlier `box 7`
scrutinee refusal becomes an acceptance case.

`dev/test-constructor-inference.py` compares 22 inferred/explicit pairs
on Bun, the Node worker and native. The pairs cover nesting, distinct
parameters, dependent fields, open variables and types, aliases, recursive
tails, nominal fields, repeated parameter constraints, a parameterless family
and parameters recovered from nominal indices. Five pairs were added after
review. `solved-slot` (`w2 (both 7 g4)`) solves A from the first parameter of
`Both`, and the second parameter holds A beneath a function former.
`pi-before-slot` (`w3 (both g4 7)`) puts the function former first.
`parameter-index` (`pt 5 (ix 7)`) has a result index that mentions a parameter.
`pi-field` (`r g 7`) has a function field before the field that solves the
parameter. `nested-section` (`wn (box g4) 7`) has a nested constructor with a
function field.

Every pair must erase identically to its explicitly annotated form. Every pair
must also give the same checked output, or it must have a golden. A golden holds
the inferred `def value` line and its reason. The harness accepts a golden only
when that line is the only line that differs. There are two golden classes:

- Inner annotations on nested constructor arguments, in nine pairs: `nested`,
  `nominal-field`, `nominal-nested`, `nominal-repeated`, `value-parameter`,
  `solved-slot`, `pi-before-slot`, `parameter-index` and `nested-section`. A
  nested constructor argument is inferred, so it keeps its checked annotation.
- The alias `N` unfolds to `Nat`, in the pair `alias`. The explicit type names
  `N`, and inference gives the unfolded type `Nat`.

Each comparison records four sha256 values: `inferred_checked_sha256`,
`explicit_checked_sha256`, `inferred_erased_sha256` and
`explicit_erased_sha256`. It also records `checked_equal`, `golden_reason` and
`erasure_equal`. The harness computes `erasure_equal` from the two erased
outputs. The 8 refusal cases cover missing evidence, inconsistent
constraints, arity, dependent-field typing, linear usage and a lambda in a field
that holds an unsolved parameter (`section-field`). The public example runs in
checked and erased modes. The resulting 294 observations and
7 semantic mutation kills are recorded in
`stage-a-constructor-inference.json`.

All 7 mutants compile before their positive witness is refused. They
merge distinct inference slots, discard field constraints, discard nominal
parameter constraints, discard nominal index constraints, remove the final
annotation, drop the reversal of the parameter values in the final annotation
(`drop-reverse`, killed by `parameter-index`) or scan only the top of a quoted
field type for a slot (`occurs-scan`, killed by `solved-slot`). The existing
constructor suite retains its six mutation obligations. Two mutation witnesses
now use nullary parameterized constructors, since field inference makes their
earlier box witnesses valid.

The source policy registers three host-only unsafe functions: the work-list
traversal `open_term`, the constraint loop `solve` and the resumption wrapper
`inferable`. It also registers six catch-all functions with exact site counts.
The elaborator keeps its existing 640-line budget. The new module has a 220-line
budget and contains 219 lines.

Validation commands:

- `python3 -P dev/build.py --check`: pass.
- `python3 -P dev/house-bend.py`: pass, 54 files, 43 line budgets, 63 registered
  unsafe sites and 41 registered catch-all functions.
- `python3 -P dev/test-constructor-inference.py --mutations`: pass on all three
  hosts, 22 pairs, 8 refusals, 294 observations
  and 7 mutation kills.
- `make gates`: pass (exit 0), including the full earlier gate sequence, the
  three-host constructor suites, pin checks and the final Bend source check.
- The existing constructor suite: 57 cases, 282 observations, 37 acceptance
  goldens and six semantic mutation kills on all three hosts.
- The public CLI: 93 corpus files, 292 command observations on all three hosts.
  The pinned program gate also checks all 245 files in both modes on Bun, the
  Node worker and native.

The final integration records are
`stage-a-constructor-inference-parameters.json` and
`stage-a-constructor-inference-cli.json`. Complete gate streams are
`stage-a-constructor-inference-gates.stdout.log` and
`stage-a-constructor-inference-gates.stderr.log`. All records and logs are
produced in a copy of the staged tree of the checkout
`/Users/oobi/Documents/sole-comb`. The copy starts from
`git checkout-index -a --prefix=<copy>/` of that checkout. The source change is
made in the copy, and the records are produced there. Their source hashes match
the published source files. After this check, the records and logs are copied
into `dev/validation/` of the checkout and staged with the source change. The
publication record is produced in a second copy. That copy starts from
`git archive HEAD` of the checkout. Each file of the final copy that is
different from `HEAD` is copied into it, and each file that the final copy does
not contain is removed. Thus the second copy holds the final published bytes.
The record verifies all copied bytes, build-cache source hashes, the cached
build and six checks of the public example in the second copy (checked and
erased modes on all three hosts).

Inference reads the fields from left to right. A field type that still holds an
unsolved parameter, in any position, is not used as an expected type. The
argument is inferred, and its type solves the parameters. A parameter beneath a
function, product or sum former gets no value from that field. Another field
must supply it, and the kernel check of the final annotation covers that field.
An argument without a type of its own, such as a lambda or a tuple without an
annotation, is refused in such a field. The refusal names the constructor and
the field. Such an argument is accepted after a field that solves its parameter.
Nullary and phantom parameters still require an expected type. These cases can
use explicit annotations. Structural recursor sugar, recursive definitions,
family default arms, full erased-corpus integration, WebAssembly execution and
the full kernel differential remain pending.
This increment starts the UAT.0 constructor requirement probes; UAT revision
pinning, theorem selection and transitive axiom inventory remain pending.
