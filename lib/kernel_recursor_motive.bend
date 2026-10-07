# Checked motive and induction-hypothesis types for direct recursive layouts.
import Base
import ./foundation.bend as F
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
type plan<-C: Data> is Data:
  Plan{layout: Rec.plan<C>, level: L.t, hypotheses: List<&2, hypothesis>}

# Parameters are stored in declaration order, with the last at index zero.
def legs(tele: P.telescope) -> List<&2, T.leg>:
  match tele:
    case []: []
    case F.Pair2{_, F.Pair2{_, _}} <> +rest:
      T.Leg{[], T.Var{R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, rest)}} <> legs(rest)

def diagram(~C: Data, ~op: R.check_ops<C>, +params: P.telescope, c: C) -> V.closure:
  V.Closure{R.o_env(C, R.c_runtime(C, op), c), T.Sec{S.SColl{R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, params)}, legs(params)}}

# Keep the motive's parameter environment separate from the constructor fields.
def hypotheses(~C: Data, ~op: R.check_ops<C>, +origin: C, +mo: T.motive, fs: List<&2, Rec.field>) -> R.program<C, List<&2, hypothesis>>:
  match fs:
    case []: R.Pure{[]}
    case Rec.Field{_, _, _, _, _, Rec.Ordinary{}} <> rest: hypotheses(C, op, origin, mo, rest)
    case Rec.Field{position, _, name, _, value, Rec.Direct{indices}} <> rest:
      R.pbind(C, V.t, List<&2, hypothesis>, R.mu_result(C, op, origin, mo, indices, value), ty =>
        R.pmap(C, List<&2, hypothesis>, List<&2, hypothesis>, tail => Hypothesis{position, name, ty} <> tail, hypotheses(C, op, origin, mo, rest)))

def checked(~C: Data, ~op: R.check_ops<C>, +origin: C, +mo: T.motive, lvl: L.t, +raw: Rec.plan<C>) -> R.program<C, plan<C>>:
  match raw:
    case Rec.Plan{_, fs}: R.pmap(C, List<&2, hypothesis>, plan<C>, hs => Plan{raw, lvl, hs}, hypotheses(C, op, origin, mo, fs))

def motive(~C: Data, ~op: R.check_ops<C>, +origin: C, +owner: String, +fam: P.family, +mo: T.motive, raw: Rec.plan<C>) -> R.program<C, plan<C>>:
  +count = R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, R.family_indices(fam))
  R.pbind(C, T.motive, plan<C>, R.mu_motive_view(C, owner, count, count, mo, mo), valid =>
    R.pbind(C, L.t, plan<C>, R.mu_motive_lvl(C, op, origin, owner, fam, valid, diagram(C, op, R.family_params(fam), origin), Some{R.family_level(fam)}, R.o_env(C, R.c_runtime(C, op), origin)), +lvl =>
      R.pbind(C, Unit, plan<C>, R.mu_large(C, owner, fam, lvl), _ => checked(C, op, origin, mo, lvl, raw))))

# Motives are scoped under declaration parameters, then indices, then self.
# This plan does not introduce body binders or evaluate a recursive call.
def layout(~C: Data, ~op: R.check_ops<C>, +c: C, +scope: Rec.scope, +owner: String, +key: String, mo: T.motive) -> R.program<C, plan<C>>:
  R.pbind(C, P.family, plan<C>, R.mu_family(C, op, c, owner), +fam =>
    R.pbind(C, Rec.plan<C>, plan<C>, Uniform.layout(C, op, c, scope, owner, key), raw =>
      R.pbind(C, C, plan<C>, Rec.open(C, op, scope, R.family_params(fam), c), origin => motive(C, op, origin, owner, fam, mo, raw))))
