# Checked motive and induction-hypothesis types for direct recursive layouts.
import Base
import ./foundation.bend as F
import ./kernel_check.bend as C
import ./kernel_level.bend as L
import ./kernel_positivity.bend as P
import ./kernel_quantity.bend as Q
import ./kernel_recursor.bend as Rec
import ./kernel_recursor_uniform.bend as Uniform
import ./kernel_rules.bend as R
import ./kernel_shape.bend as S
import ./kernel_term.bend as T
import ./kernel_value.bend as V

type hypothesis is Data:
  Hypothesis{position: F.Int63.t, name: String, domain: V.t}
type plan is Data:
  Plan{layout: Rec.plan, level: L.t, hypotheses: List<&2, hypothesis>}

# Parameters are stored in declaration order, with the last at index zero.
def legs(tele: P.telescope) -> List<&2, T.leg>:
  match tele:
    case []: []
    case F.Pair2{_, F.Pair2{_, _}} <> +rest:
      T.Leg{[], T.Var{R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, rest)}} <> legs(rest)

def diagram(+params: P.telescope, c: C.ctx) -> V.closure:
  V.Closure{C.env(c), T.Sec{S.SColl{R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, params)}, legs(params)}}

# Keep the motive's parameter environment separate from the constructor fields.
def hypotheses(+origin: C.ctx, +mo: T.motive, fs: List<&2, Rec.field>) -> R.result(List<&2, hypothesis>):
  match fs:
    case []: Done{[]}
    case Rec.Field{_, _, _, _, _, Rec.Ordinary{}} <> rest: hypotheses(origin, mo, rest)
    case Rec.Field{position, _, name, _, value, Rec.Direct{indices}} <> rest:
      R.bind(V.t, List<&2, hypothesis>, Rec.rules(V.t, origin, R.mu_result(C.ctx, C.ops, origin, mo, indices, value)), ty =>
        R.map_result(List<&2, hypothesis>, List<&2, hypothesis>, tail => Hypothesis{position, name, ty} <> tail, hypotheses(origin, mo, rest)))

def checked(+origin: C.ctx, +mo: T.motive, lvl: L.t, +raw: Rec.plan) -> R.result(plan):
  match raw:
    case Rec.Plan{_, fs}: R.map_result(List<&2, hypothesis>, plan, hs => Plan{raw, lvl, hs}, hypotheses(origin, mo, fs))

def motive(+origin: C.ctx, +owner: String, +fam: P.family, +mo: T.motive, raw: Rec.plan) -> R.result(plan):
  +count = R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, R.family_indices(fam))
  R.bind(T.motive, plan, Rec.rules(T.motive, origin, R.mu_motive_view(C.ctx, owner, count, count, mo, mo)), valid =>
    R.bind(L.t, plan, Rec.rules(L.t, origin, R.mu_motive_lvl(C.ctx, C.ops, origin, owner, fam, valid, diagram(R.family_params(fam), origin), Some{R.family_level(fam)}, C.env(origin))), +lvl =>
      R.bind(Unit, plan, Rec.rules(Unit, origin, R.mu_large(C.ctx, owner, fam, lvl)), _ => checked(origin, mo, lvl, raw))))

# Motives are scoped under declaration parameters, then indices, then self.
# This plan does not introduce body binders or evaluate a recursive call.
def layout(+c: C.ctx, +owner: String, +key: String, mo: T.motive) -> R.result(plan):
  R.bind(P.family, plan, Rec.rules(P.family, c, R.mu_family(C.ctx, C.ops, c, owner)), +fam =>
    R.bind(Rec.plan, plan, Uniform.layout(c, owner, key), raw =>
      R.bind(C.ctx, plan, Rec.parameters(R.family_params(fam), C.make(C.globals(c), C.budget(c))), origin => motive(origin, owner, fam, mo, raw))))
