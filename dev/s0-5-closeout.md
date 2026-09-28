# S0-5 closeout

Adopted on 2026-09-27. The subsequent [Stage A entry decision](stage-a-entry.md)
supersedes this document's scratch-first work order. Further scratch compiler
features are retired; real Stage A implementation is active under `lib/`.
The bounded scope below records the former plan and the outstanding R2
qualification requirements. Historical validation claims remain unchanged.

## Frozen workloads

The source identities and scratch copies are in [r2-workloads.json](r2-workloads.json).
The selected small workload is the plan's `pilot/bend2/smoke.bend` alternative,
including its complete `core.bend` import. The conversion workload is that
same complete `core.bend`, including all four obligations:

- `lookup_head`: lookup at zero reduces to the head.
- `closed_var_impossible`: a bound in an empty environment is impossible.
- `lookup_tail`: successor lookup reduces to lookup in the tail.
- `let_bound_value`: the fuelled machine reduces the bound variable to its value.

The conversion entry is `conversion.bend`, containing only
`import ./core.bend as Core`. Direct entry through the original core collides
on `Halt` under the pinned host. The namespace wrapper preserves core byte
for byte. Its qualification includes rejection of a false imported law.

Keep every core declaration. The small workload adds the original nested-let
`main`, with fuel 9 and values 7 and 9. Preserve U32 values, dependent bounds,
environment lengths, the indexed `Term`, explicit exhaustion, and the laws.
The pinned Bend `Base` dependency remains in the denominator import closure.
Use the plan's NAT-MU representation for `Nat` on the sole side. The scratch
checker keeps `U32` as a builtin type with literals. This is scratch scope
only. The plan adds U32 at M2 under the ruling U32-BUILTIN, and the M0
surface has no U32 type. A U32-free M0 twin needs a documented workload
revision.

The Bend inputs are copied byte for byte to
`/private/tmp/claude/kan-elim-lang-m0/r2-risk/workloads-v1/`.
The [complete sole translations](validation/s0-5-r2-twins.md) now have a
reviewed declaration map and passing lexical checks. Their source checker
qualification remains pending. The freeze supplies no R2 result.
Selecting this existing alternative avoids the `elab_state.bend` import
closure. It does not change the denominator command, thresholds, or host pin.

## Finite work queue

Execute these units in order. Each unit records source hashes, checks, and
the next unmet acceptance condition. New scratch features need a concrete
requirement in the frozen twins or a failing qualification case.

| Unit | Required result | Completion evidence |
|---|---|---|
| Step 1, translate | Write full `core.sole` and `smoke.sole` with a declaration and law map to the frozen Bend files. Flatten the core import if needed, and include its checking cost in each measured sole input. Use explicit motives, all positional arms, and constructor eliminations. | Every source declaration and all four obligations are accounted for; source hashes and intended positive and negative verdicts are recorded. Review the mapping before another checker feature slice. |
| Step 2, close the checker | Derive data metadata from source; support the indexed and recursive declarations used by `Term`, `Job`, `Kont`, and `Env.Cell`; support the Nat recursors for `Bound`, `Env`, `lookup`, and `run`, dependent elimination, scratch-only U32 literals, and equality/reflexivity checking. Preserve the required K1/K1d behavior (M0-PLAN 5.3 and 5.4) on reachable recursors. | The integrated source checker accepts both twins and refuses ill-typed constructors, invalid bounds, and false laws. Run relevant existing component regressions. It must compute the verdict from source, with no filename dispatch, source-hash whitelist, hard-coded result, or prechecked metadata. |
| Step 3, qualify the command | Add portable file input and the existing probe command contract. Supply empty input and lex, parse, type, and conversion refusals that reach their named phases. Locate and qualify the reference startup inputs and copied bundles. | `r2-prepare` produces its hash-bound `run-plan.json` after correctness checks on Bun and the pinned Node worker; record native build/results as INFO. Preserve the preparer's endpoint eligibility rules. |
| Step 4, measure and close | Run the existing serial measurement runner and evidence assembler on the qualified artifacts. | Valid per-workload samples, startup legs and shares, pinned hashes, endpoint selection, and any required GREEN confirmation are recorded. Produce `dev/r2-risk.json` before Stage A. |

The existing lexer, parser, resolver, conversion, typing, arm, and data
records are reusable component evidence. The data checker still consumes
non-indexed caller metadata. The workload parser now retains source data
declarations and index telescopes, but checked metadata derivation and
indexed checking remain step 2 gaps.

Step 1 completed on 2026-09-27. Its [record](validation/s0-5-r2-twins.json)
accounts for all 19 core declarations, the smoke main, and all four
obligations. Both complete twins pass lexing and exact syntax-tree checks;
their intended ACCEPT verdicts and the three negative fixtures' intended
REFUSE verdicts remain pending.
Step 2 is the next unmet acceptance condition. Its concrete syntax and
checker requirements are in the [translation review](validation/s0-5-r2-twins.md).

The [workload parser record](validation/s0-5-r2-workload-parser.md) closes
the grammar prerequisite of step 2. It preserves all 20 declarations in
`core.sole`, all 22 in `smoke.sole`, and each appended negative declaration.
These counts describe sole syntax; the step 1 map accounts for the 19 Bend
core declarations. The next slice must first resolve the new syntax and
derive checked data metadata from it. The step 2 row above and the
translation review give the other step 2 requirements. All three negative
fixtures intentionally parse.

## Deferred scratch work

Records, projection syntax, `else` expansion and its recursive traversal,
and motive inference are outside this closeout. The translations use
constructor eliminations, complete arms, and explicit convoy motives.
General eta/proof-irrelevance work, extra indexed forms, and broad prelude
coverage wait unless step 1 demonstrates a concrete need. The frozen core's
indexed terms and equality laws cannot be removed to avoid implementing them.

Changing a seed, dropping a declaration or obligation, changing a literal
representation, or changing the measured pipeline requires a documented
workload revision and fresh qualification. An observed failure is not
permission to weaken the workload or gate.

## Exit and handoff

Keep the current child-CPU method, per-workload thresholds, load ceiling,
sample count, startup measurements, and confirmation rules. A selected
eligible endpoint with GREEN or AMBER evidence satisfies the R2 runtime gate.
GREEN is at most 0.45; AMBER is above 0.45 and at most 1.0. FAIL above 1.0
requires a written user ruling. Unmet measurement conditions, unresolved
endpoint selection, and missing evidence remain unfinished. The entry
decision moves this gate to the operational real compiler before M0 closure.

Start the [Stage A compiler milestones](stage-a-milestones.md) now.
The scratch implementation remains outside the repository. Stage A writes
the actual pinned Kanon kernel port under `lib/`; later stages add the
sole surface and Wasm backend.
