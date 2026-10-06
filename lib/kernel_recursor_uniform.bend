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
# context, since an actual child parameter can mention a constructor field. The child parameter i is
# compared with the expected value i of the scope: its abstract level, or its instance value.
def arguments(+c: C.ctx, +scope: Rec.scope, tele: P.telescope, values: List<&2, V.t>, expected: List<&2, V.t>, +prefix: C.ctx) -> R.result(Unit):
  match tele, values:
    case [], []: Done{Unit{}}
    case [], _ <> _: Fail{E.Mismatch{"recursive parameter arity differs from the family telescope"}}
    case _ <> _, []: Fail{E.Mismatch{"recursive parameter arity differs from the family telescope"}}
    case F.Pair2{q, F.Pair2{x, ty}} <> rest, got <> tail:
      match expected:
        case []: Fail{E.Mismatch{"recursive parameter arity differs from the family telescope"}}
        case +e <> es:
          R.bind(V.t, Unit, EV.eval(C.globals(prefix), C.env(prefix), ty), +tyv =>
            R.bind(Bool, Unit, Rec.rules(Bool, c, R.c_conv(C.ctx, C.ops, c, tyv, got, e)), eq =>
              Q.if_else(R.result(Unit), eq,
                _ => arguments(c, scope, rest, tail, es, Rec.install(scope, x, q, tyv, e, prefix)),
                _ => Fail{mismatch()})))

def diagram(+base: C.ctx, +scope: Rec.scope, +c: C.ctx, params: P.telescope, expected: List<&2, V.t>, view: Maybe<&2, V.Former>) -> R.result(Unit):
  match view:
    case None{}: Fail{E.Mismatch{"a direct recursive field has no family former"}}
    case Some{V.Former{_, d, _}}:
      R.bind(List<&2, V.t>, Unit, Rec.rules(List<&2, V.t>, c, R.mu_param_env(C.ctx, C.ops, c, d)), env =>
        arguments(c, scope, params, R.rev_append(V.t, env, []), expected, base))

def fields(+base: C.ctx, +scope: Rec.scope, +c: C.ctx, +params: P.telescope, +expected: List<&2, V.t>, values: List<&2, Rec.field>) -> R.result(Unit):
  match values:
    case []: Done{Unit{}}
    case Rec.Field{_, _, _, _, _, Rec.Ordinary{}} <> rest: fields(base, scope, c, params, expected, rest)
    case Rec.Field{_, _, _, ty, _, Rec.Direct{_}} <> rest:
      R.bind(Unit, Unit, diagram(base, scope, c, params, expected, V.as_lan(ty)), _ => fields(base, scope, c, params, expected, rest))

# The expected value of each parameter in a layout: the level of its abstract local, or its instance value.
def levels(tele: P.telescope, +level: F.Int63.t) -> List<&2, V.t>:
  match tele:
    case []: []
    case _ <> rest: V.var(level) <> levels(rest, F.Int63.succ(level))

def targets(scope: Rec.scope, tele: P.telescope, +c: C.ctx) -> List<&2, V.t>:
  match scope:
    case Rec.Abstract{}: levels(tele, C.size(c))
    case Rec.Instance{values}: values

# The expected list is built once per layout.
def validate(+base: C.ctx, +scope: Rec.scope, +params: P.telescope, +value: Rec.plan) -> R.result(Rec.plan):
  match value:
    case Rec.Plan{c, fs}: R.map_result(Unit, Rec.plan, _ => value, fields(base, scope, c, params, targets(scope, params, base), fs))

# This checks parameter compatibility, not a recursor typing certificate.
def layout(+c: C.ctx, +scope: Rec.scope, +owner: String, key: String) -> R.result(Rec.plan):
  R.bind(P.family, Rec.plan, Rec.rules(P.family, c, R.mu_family(C.ctx, C.ops, c, owner)), fam =>
    R.bind(Rec.plan, Rec.plan, Rec.layout(c, scope, owner, key), plan => validate(c, scope, R.family_params(fam), plan)))
