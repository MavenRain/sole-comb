# Uniform-parameter validation for direct recursive-field layouts.
import Base
import ./foundation.bend as F
import ./kernel_error.bend as E
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
def arguments(~C: Data, ~op: R.check_ops<C>, +c: C, +scope: Rec.scope, tele: P.telescope, values: List<&2, V.t>, expected: List<&2, V.t>, +prefix: C) -> R.program<C, Unit>:
  match tele, values:
    case [], []: R.Pure{Unit{}}
    case [], _ <> _: R.Abort{E.Mismatch{"recursive parameter arity differs from the family telescope"}}
    case _ <> _, []: R.Abort{E.Mismatch{"recursive parameter arity differs from the family telescope"}}
    case F.Pair2{q, F.Pair2{x, ty}} <> rest, got <> tail:
      match expected:
        case []: R.Abort{E.Mismatch{"recursive parameter arity differs from the family telescope"}}
        case +e <> es:
          R.pbind(C, V.t, Unit, R.c_eval(C, op, prefix, ty), +tyv =>
            R.pbind(C, Bool, Unit, R.c_conv(C, op, c, tyv, got, e), eq =>
              R.pchoose(C, Unit, eq,
                _ => arguments(C, op, c, scope, rest, tail, es, Rec.install(C, op, scope, x, q, tyv, e, prefix)),
                _ => R.Abort{mismatch()})))

def diagram(~C: Data, ~op: R.check_ops<C>, +base: C, +scope: Rec.scope, +c: C, params: P.telescope, expected: List<&2, V.t>, view: Maybe<&2, V.Former>) -> R.program<C, Unit>:
  match view:
    case None{}: R.Abort{E.Mismatch{"a direct recursive field has no family former"}}
    case Some{V.Former{_, d, _}}:
      R.pbind(C, List<&2, V.t>, Unit, R.mu_param_env(C, op, c, d), env =>
        arguments(C, op, c, scope, params, R.rev_append(V.t, env, []), expected, base))

def fields(~C: Data, ~op: R.check_ops<C>, +base: C, +scope: Rec.scope, +c: C, +params: P.telescope, +expected: List<&2, V.t>, values: List<&2, Rec.field>) -> R.program<C, Unit>:
  match values:
    case []: R.Pure{Unit{}}
    case Rec.Field{_, _, _, _, _, Rec.Ordinary{}} <> rest: fields(C, op, base, scope, c, params, expected, rest)
    case Rec.Field{_, _, _, ty, _, Rec.Direct{_}} <> rest:
      R.pbind(C, Unit, Unit, diagram(C, op, base, scope, c, params, expected, V.as_lan(ty)), _ => fields(C, op, base, scope, c, params, expected, rest))

# The expected value of each parameter in a layout: the level of its abstract local, or its instance value.
def levels(tele: P.telescope, +level: F.Int63.t) -> List<&2, V.t>:
  match tele:
    case []: []
    case _ <> rest: V.var(level) <> levels(rest, F.Int63.succ(level))

def targets(~C: Data, ~op: R.check_ops<C>, scope: Rec.scope, tele: P.telescope, +c: C) -> List<&2, V.t>:
  match scope:
    case Rec.Abstract{}: levels(tele, R.c_size(C, op, c))
    case Rec.Instance{values}: values

# The expected list is built once per layout.
def validate(~C: Data, ~op: R.check_ops<C>, +base: C, +scope: Rec.scope, +params: P.telescope, +value: Rec.plan<C>) -> R.program<C, Rec.plan<C>>:
  match value:
    case Rec.Plan{c, fs}: R.pmap(C, Unit, Rec.plan<C>, _ => value, fields(C, op, base, scope, c, params, targets(C, op, scope, params, base), fs))

# This checks parameter compatibility, not a recursor typing certificate.
def layout(~C: Data, ~op: R.check_ops<C>, +c: C, +scope: Rec.scope, +owner: String, key: String) -> R.program<C, Rec.plan<C>>:
  R.pbind(C, P.family, Rec.plan<C>, R.mu_family(C, op, c, owner), fam =>
    R.pbind(C, Rec.plan<C>, Rec.plan<C>, Rec.layout(C, op, c, scope, owner, key), plan => validate(C, op, c, scope, R.family_params(fam), plan)))
