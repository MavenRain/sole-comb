# Uniform-parameter validation for direct recursive-field layouts.
import Base
import ./foundation.bend as F
import ./kernel_check.bend as C
import ./kernel_error.bend as E
import ./kernel_eval.bend as EV
import ./kernel_positivity.bend as P
import ./kernel_quantity.bend as Q
import ./kernel_recursor.bend as Rec
import ./kernel_rules.bend as R
import ./kernel_value.bend as V

def mismatch() -> E.t:
  E.Not_yet{"recursor layout requires uniform recursive parameters"}

# Parameter domains use the given context and the declaration prefix. Conversion uses the full field
# context, since an actual child parameter can mention a constructor field.
def arguments(+c: C.ctx, tele: P.telescope, values: List<&2, V.t>, +prefix: C.ctx) -> R.result(Unit):
  match tele, values:
    case [], []: Done{Unit{}}
    case [], _ <> _: Fail{E.Mismatch{"recursive parameter arity differs from the family telescope"}}
    case _ <> _, []: Fail{E.Mismatch{"recursive parameter arity differs from the family telescope"}}
    case F.Pair2{q, F.Pair2{x, ty}} <> rest, got <> tail:
      R.bind(V.t, Unit, EV.eval(C.globals(prefix), C.env(prefix), ty), +tyv =>
        R.bind(Bool, Unit, Rec.rules(Bool, c, R.c_conv(C.ctx, C.ops, c, tyv, got, V.var(C.size(prefix)))), eq =>
          Q.if_else(R.result(Unit), eq,
            _ => arguments(c, rest, tail, C.bind(x, q, tyv, prefix)),
            _ => Fail{mismatch()})))

def diagram(+base: C.ctx, +c: C.ctx, params: P.telescope, view: Maybe<&2, V.Former>) -> R.result(Unit):
  match view:
    case None{}: Fail{E.Mismatch{"a direct recursive field has no family former"}}
    case Some{V.Former{_, d, _}}:
      R.bind(List<&2, V.t>, Unit, Rec.rules(List<&2, V.t>, c, R.mu_param_env(C.ctx, C.ops, c, d)), env =>
        arguments(c, params, R.rev_append(V.t, env, []), base))

def fields(+base: C.ctx, +c: C.ctx, +params: P.telescope, values: List<&2, Rec.field>) -> R.result(Unit):
  match values:
    case []: Done{Unit{}}
    case Rec.Field{_, _, _, _, _, Rec.Ordinary{}} <> rest: fields(base, c, params, rest)
    case Rec.Field{_, _, _, ty, _, Rec.Direct{_}} <> rest:
      R.bind(Unit, Unit, diagram(base, c, params, V.as_lan(ty)), _ => fields(base, c, params, rest))

def validate(+base: C.ctx, params: P.telescope, +value: Rec.plan) -> R.result(Rec.plan):
  match value:
    case Rec.Plan{c, fs}: R.map_result(Unit, Rec.plan, _ => value, fields(base, c, params, fs))

# This checks parameter compatibility, not a recursor typing certificate.
def layout(+c: C.ctx, +owner: String, key: String) -> R.result(Rec.plan):
  R.bind(P.family, Rec.plan, Rec.rules(P.family, c, R.mu_family(C.ctx, C.ops, c, owner)), fam =>
    R.bind(Rec.plan, Rec.plan, Rec.layout(c, owner, key), plan => validate(c, R.family_params(fam), plan)))
