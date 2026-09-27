# S0-5 complete workload translations

Step 1 of [the closeout](../s0-5-closeout.md) is complete. The scratch
`core.sole` and `smoke.sole` files retain the frozen workloads and all four
obligations. The [record](s0-5-r2-twins.json) binds the inputs, translations,
declaration map, negative fixtures, audit script, and lexer results by hash.
It reports structural and lexical checks, not sole compiler acceptance.

## Sources and mapping review

The sources are under
`/private/tmp/claude/kan-elim-lang-m0/r2-risk/workloads-v1/`.
The frozen Bend hashes still match [the manifest](../r2-workloads.json).
The seed correctness record also retains its frozen hash. This unit reuses
that record; it does not rerun or replace the Bend seed qualification.

The record maps all 19 top-level declarations in `core.bend` to 13 groups.
It pairs each Bend signature law with its filled definition. It also maps
`smoke.bend:4-6` to the flattened core, the `nine` helper, and `main`.
Its `declaration_map` gives source and target line ranges for each group.
Each range ends on the line before the next declaration, so it includes
trailing blank and comment lines.

| Bend declaration | Sole declaration | Mapping review |
|---|---|---|
| Bound signature and definition | Bound | Nat elimination computes Empty at zero and Unit at the head; the predecessor IH computes the tail bound. |
| Env signature and definition | Env | Nat elimination computes Unit at zero and a cell over the predecessor environment at successor. |
| Env.Cell | EnvCellF, EnvCell | Cell retains head and tail, in order; the indexed alias supplies Env n as the tail type. |
| Term | Term | Lit, Var, Let, Ann retain their order; Var retains Bound n index; Let retains Term (succ n) for its body. |
| Job | Job | Eval retains n, term, and env; Return retains its U32 value. |
| Kont | Kont | Bind retains the successor-length body, environment, and next continuation. |
| Outcome | Outcome | Done and Exhausted remain separate constructors. |
| lookup | lookup | Explicit convoy motives retain dependent environments and bounds. |
| run | run | The Nat fuel IH drives all five recursive transitions; zero fuel produces Exhausted. |
| lookup_head signature and proof | lookup_head | The intended U32 equality reduces to the head, with a refl proof. |
| closed_var_impossible signature and proof | closed_var_impossible | Empty elimination consumes Bound zero index. |
| lookup_tail signature and proof | lookup_tail | The intended U32 equality reduces to predecessor lookup, with a refl proof. |
| let_bound_value signature and proof | let_bound_value | The original five-fuel computation has intended result Done value, with a refl proof. |

The author's mapping review preserves each source declaration and
obligation. The intended equalities still need the integrated checker in
step 2.

`EnvCellF Tail` has one constructor with fields `U32` and `Tail`.
`EnvCell n` is the alias `EnvCellF (Env n)`. This factors the original
forward dependency between `Env.Cell` and `Env` without a source postulate
or a recursive definition. `Env (succ n)` reduces to `EnvCellF (Env n)`
through the Nat IH, and `EnvCell n` unfolds to the same term. The alias
remains a checked declaration in both workloads.

The Base mapping declares Nat as the required unary mu, Empty as `sum ()`,
Unit as `prod ()`, and its inhabitant as `tuple ()`. Id is an indexed mu
with a reflexivity constructor. U32 remains the scratch-only builtin from
the closeout. `five` and `nine` use succ and zero; the smoke U32 values stay
7 and 9. The source contains no new axioms or postulates.

Term constructor lengths are explicit arguments. The Term elimination
uses `as currentTerm in Term m return Env m -> Kont -> Outcome` so that
the environment follows the constructor index. This is the explicit
indexed motive from the inherited Kanon surface, combined with the R4
ordinary-term arms. The current scratch parser does not support this form.
The twins use unmarked binders; this record makes no quantitative-usage or
erasure-equivalence claim.

All eliminations carry explicit motives and complete positional arms.
The Let arm binds both recursive IHs; Ann and Bind each bind one. These
IHs are unused. The inner Nat eliminations also bind unused IHs. Step 2
must keep them delayed under K1d. The outer Nat IHs implement Bound, Env,
lookup, and run; the source contains no recursive definition syntax.

`smoke.sole` begins with the complete, byte-identical `core.sole`. Thus
its checking cost includes every core declaration and obligation. The
added main retains the two nested lets, fuel nine, values 7 and 9, and
lookup at successor zero. Its intended result is `Done 7`.

## Checks and intended verdicts

The record reports 18 passing structural checks. Four audit mutations
remove an obligation, remove a required IH binder, remove the core prefix,
or change the smoke value. The audit detects all four. These checks verify
the translation contract; they do not parse or typecheck the source.

The existing scratch lexer accepts both complete inputs on Bun and the
pinned Node worker. The native result is INFO. The final run is
`/private/tmp/sole-comb-step1-20260927/lexing-003/validation.json`;
the record embeds that result and binds its source and log hashes.
This check covers lexing success, not a golden token stream or a parser AST.

| Input | Intended verdict | Reason or intended result |
|---|---|---|
| core.sole | ACCEPT | Every declaration and all four obligations check. |
| smoke.sole | ACCEPT | The complete core checks; main reduces to Done 7. |
| bad-constructor.sole | REFUSE, typing | Lit receives unit where it expects U32. |
| bad-bound.sole | REFUSE, typing | A variable at length zero receives unit for an Empty bound. |
| false-law.sole | REFUSE, conversion | refl (Done 7) cannot prove equality with Exhausted. |

All five compiler verdicts remain PENDING. Each negative fixture contains
the complete core followed by one offending definition. The record binds
their full source hashes. No filename or source-hash whitelist may supply
these verdicts when the checker implements step 2.

`make gates` passed in the isolated repository copy at
`/private/tmp/sole-comb-step1-20260927/repo`: 143 regression tests, pin
checks, and the skeleton build check. The full outputs are
[stdout](s0-5-r2-twins-gates.stdout) and
[stderr](s0-5-r2-twins-gates.stderr). These gates do not check sole twins.

## Reproduction and next step

Run the source audit against a passing, hash-matching lexing record:

```sh
python3 -P /private/tmp/claude/kan-elim-lang-m0/r2-risk/workloads-v1/audit-twins.py \
  --repo /Users/oobi/Documents/sole-comb \
  --lexing /private/tmp/sole-comb-step1-20260927/lexing-003/validation.json \
  --output /private/tmp/sole-comb-step1-audit.json
```

To rerun lexing, use `validate-twin-lexing.py` beside the audit, with
`--repo /Users/oobi/Documents/sole-comb` and `--output` naming a new scratch
directory. The existing harness refuses an output directory that exists.

Step 2 now has concrete source requirements: brace-form mu declarations,
parameters and dependent index telescopes, sum/prod/tuple at arity zero,
zero-arm elimination of `sum ()` after `Bound zero i` reduces to Empty,
scratch U32 literals, indexed motives with `in`, constructor checking,
source-derived data metadata, K1/K1d recursion, and Id/reflexivity checking.
The checker must compute the positive and negative verdicts from source.

By reading, the four obligations need only beta, delta, and K1 iota
reduction. The translations show no concrete need for eta or
proof-irrelevance rules or broad prelude coverage. The indexed forms that
they need are in the list above. Eta, proof irrelevance, records,
projections, default arms, and motive inference remain deferred. The step 2
checker must confirm this reading.
Portable input, command qualification, startup legs, timing samples,
endpoint selection, and `dev/r2-risk.json` remain pending.

## Review fixes, 2026-09-27

An independent review examined the declaration map, the obligations, the
negative fixtures, the record, and these documents. It found no defect in
the map, the obligations, or the fixtures. It found four prose defects,
fixed as follows:

- The README status paragraph no longer says that the translations are
  pending.
- The mapping review names its author, and this section records the
  independent review.
- The step 2 requirements include the zero-arm elimination of `sum ()`.
  `dev/r2-prepare.md` points to them and no longer gives a shorter list.
- This record states the step 1 outcome for eta, proof irrelevance, and
  prelude coverage. `dev/r2-prepare.md` no longer waits on step 1 for them.

The review also made the `EnvCell` reduction sentence exact and documented
the range convention of `declaration_map`. The `ascii` structural check
cannot fail by itself: the audit reads both sources as ASCII and stops on
any other byte first. These edits change prose only. The record and
manifest hashes are unchanged.
