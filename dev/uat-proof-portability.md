# UAT proof portability priority

Added on 2026-10-02 at the user's request to schedule the prerequisites for
porting unified-aggregation-theory to sole-comb soon. Preparation starts with
the next Stage A public-source increment. Implementation is required early M1
work, ahead of new seed breadth, lex2 and conditional brec2 work. Existing M1
acceptance and CPU performance obligations remain required.

## Sequence and acceptance

| Increment | Placement and deliverable | Required evidence |
|---|---|---|
| UAT.0 | Next Stage A public-source increment: pin UAT and dependency revisions, theorem statements and transitive axiom assumptions; record the current source refusals. Prioritize the already planned public structural recursors and unconstrained parameterized-constructor inference. | Reproducible probes and a requirement-to-milestone map, distinguishing current refusals from landed capabilities. Structural recursion uses the M0 `elim` recursor and recursive-field induction hypotheses; `def rec` remains excluded. |
| UAT.1 | First M1 increment: admit the usual equality family in `Prop` with higher-universe indices, using the existing family machinery. Supply reflexivity, equality elimination, dependent transport and the required heterogeneous equality interface. | Check Nat-indexed and type-parameterized equality in `Prop`, transport into `Type`, and a transported dependent morphism. Refuse unequal endpoints and invalid large elimination. Review positivity, universe admission and singleton elimination together. Moving equality to `Type 0` does not satisfy this gate. |
| UAT.2 | Public dependent and parameterized records, checked constructors and dependent projections; complete the structural recursor path needed by the pilot. | Check `Category`, `Functor` and `GAction` signatures with explicit instance arguments, proof fields and dependent `Hom`/`map` fields. Check structural induction with recursive-field hypotheses and refuse nonstructural recursion. Surface expansion reaches existing checked core forms. |
| UAT.3 | Prenex universe polymorphism and bounded instance resolution, with explicit dictionary terms. | Check independently instantiated object/morphism universes and UAT's categorical signatures. Test level substitution, successor/maximum constraints, inconsistent levels, ambiguous instances, cycles and budget exhaustion. Fixed-universe specialization may help the pilot but does not complete this increment. |
| UAT.4 | Checked equality/extensionality and categorical library: function extensionality, `Functor.ext`, categories, functors, natural transformations, group actions and the universal-property definition of left Kan extension. | Kernel-check dependency-ordered proofs, including UAT's dependent `Functor.ext` and transport-heavy orbit morphism laws. Compare theorem statements and transitive axiom assumptions with the Lean baseline. Changed assumptions are disclosed and prevent a faithful-port claim. |
| UAT.5 | Checked pilot: `Z2Group`, a discrete category and action, orbit projection, and one concrete aggregation witness. | Public `.sole-comb` source checks end to end on the qualified checker hosts, preserving theorem correspondence and axiom accounting. Record the remaining Arrow dependency and rational/finite library work as the follow-on full-port backlog. |

## Semantic and proof gates

UAT.0 preparation begins during Stage A. Changes to pinned kernel semantics
begin after M0 acceptance. UAT.1 through UAT.5 form the first M1 implementation
sequence and are not deferred to M2 or M3. Each increment records focused
validation and review; the existing M1 obligations still gate M1 closure.

M0's pinned Kanon comparisons retain their corpus, required modes, divergence
policy and acceptance meaning. M1 compatibility work has a separately versioned
semantic delta and acceptance/refusal corpus. Changed results must be attributed
to that delta; reference fixtures are not rewritten to hide it. Retain the
single-eliminator design, positivity and quantity checks, typed errors, finite
budgets, and the exclusions on general recursion and negative datatypes. This
schedule implies no new trusted primitive or proof axiom.

UAT's `LeftKanExtension` carries a functor, unit, factorization and uniqueness
proofs over arbitrary categories. Define and prove its relationship to native
`Lan` where used. Matching terminology is insufficient. Lean tactics may be
replaced by checked proof terms; a general tactic engine is not a prerequisite.

Full-port readiness requires UAT.1 through UAT.5 to pass. The pilot does not
complete ArrowCat, the rational/finite prelude or every UAT theorem. Keep that
inventory explicit and require checked proofs and matching assumptions before
marking any theorem ported. The full UAT port is not an M0 closure claim.
