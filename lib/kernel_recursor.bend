# Direct recursive-field layouts. This is metadata, not a recursor certificate.
import Base
import ./foundation.bend as F
import ./kernel_error.bend as E
import ./kernel_positivity.bend as P
import ./kernel_quantity.bend as Q
import ./kernel_recursor_group.bend as Group
import ./kernel_rules.bend as R
import ./kernel_term.bend as T
import ./kernel_value.bend as V

type role is Data:
  Ordinary{}
  Direct{indices: List<&2, V.t>}
type field is Data:
  Field{position: F.Int63.t, quantity: Q.t, name: String, domain: V.t, value: V.t, role: role}
type plan<-C: Data> is Data:
  Plan{context: C, fields: List<&2, field>}

# The declaration parameters open as abstract locals, or as locals defined at the parameter values of a
# scrutinee type. A defined parameter is not a hypothesis of the motive or the bodies: its quantity is Zero.
type scope is Data:
  Abstract{}
  Instance{values: List<&2, V.t>}

def abstract(~C: Data, ~op: R.check_ops<C>, tele: P.telescope, +c: C) -> R.program<C, C>:
  match tele:
    case []: R.Pure{c}
    case F.Pair2{q, F.Pair2{x, ty}} <> rest:
      R.pbind(C, V.t, C, R.c_eval(C, op, c, ty), tyv => abstract(C, op, rest, R.c_bind(C, op, c, x, q, tyv)))

def instance(~C: Data, ~op: R.check_ops<C>, tele: P.telescope, values: List<&2, V.t>, +c: C) -> R.program<C, C>:
  match tele, values:
    case [], []: R.Pure{c}
    case [], _ <> _: R.Abort{E.Mismatch{R.wrong_pack}}
    case _ <> _, []: R.Abort{E.Mismatch{R.wrong_pack}}
    case F.Pair2{_, F.Pair2{x, ty}} <> rest, v <> tail:
      R.pbind(C, V.t, C, R.c_eval(C, op, c, ty), tyv => instance(C, op, rest, tail, R.c_define(C, op, c, x, Q.Zero{}, tyv, v)))

def open(~C: Data, ~op: R.check_ops<C>, scope: scope, tele: P.telescope, +c: C) -> R.program<C, C>:
  match scope:
    case Abstract{}: abstract(C, op, tele, c)
    case Instance{values}: instance(C, op, tele, values, c)

def parameters(~C: Data, ~op: R.check_ops<C>, tele: P.telescope, +c: C) -> R.program<C, C>: open(C, op, Abstract{}, tele, c)

def install(~C: Data, ~op: R.check_ops<C>, scope: scope, x: String, q: Q.t, tyv: V.t, v: V.t, +c: C) -> C:
  match scope:
    case Abstract{}: R.c_bind(C, op, c, x, q, tyv)
    case Instance{_}: R.c_define(C, op, c, x, Q.Zero{}, tyv, v)

def refusal(+owner: String, tm: T.t) -> E.t:
  E.Not_yet{F.choose(String, P.occurs([owner], tm), "recursor layout supports direct recursive fields only", "recursor layout does not support mutual families")}

def ordinary(~C: Data, ~op: R.check_ops<C>, +c: C, +owner: String, group: List<&2, String>, ty: V.t) -> R.program<C, role>:
  R.pbind(C, T.t, role, R.lift(C, T.t, R.o_quote(C, R.c_runtime(C, op), c, ty)), +tm =>
    R.pchoose(C, role, P.occurs(group, tm),
      _ => R.Abort{refusal(owner, tm)},
      _ => R.Pure{Ordinary{}}))

def family_role(~C: Data, ~op: R.check_ops<C>, +c: C, +owner: String, +group: List<&2, String>, +ty: V.t, view: Maybe<&2, F.Pair2<String, List<&2, V.t>>>) -> R.program<C, role>:
  match view:
    case None{}: ordinary(C, op, c, owner, group, ty)
    case Some{F.Pair2{name, indices}}:
      R.pchoose(C, role, String.eq(owner, name), _ => R.Pure{Direct{indices}}, _ => ordinary(C, op, c, owner, group, ty))

def former_role(~C: Data, ~op: R.check_ops<C>, +c: C, +owner: String, +group: List<&2, String>, +ty: V.t, view: Maybe<&2, V.Former>) -> R.program<C, role>:
  match view:
    case None{}: ordinary(C, op, c, owner, group, ty)
    case Some{V.Former{shape, _, _}}: family_role(C, op, c, owner, group, ty, R.as_mu(V.t, shape))

def prepend(-C: Data, field: field, value: plan<C>) -> plan<C>:
  match value:
    case Plan{c, tail}: Plan{c, field <> tail}

def fields(~C: Data, ~op: R.check_ops<C>, +owner: String, +group: List<&2, String>, tele: P.telescope, +c: C, +position: F.Int63.t) -> R.program<C, plan<C>>:
  match tele:
    case []: R.Pure{Plan{c, []}}
    case F.Pair2{+q, F.Pair2{+x, ty}} <> rest:
      R.pbind(C, V.t, plan<C>, R.c_eval(C, op, c, ty), tyv =>
        R.pbind(C, V.t, plan<C>, R.lift(C, V.t, R.o_whnf(C, R.c_runtime(C, op), c, tyv)), +w =>
          R.pbind(C, role, plan<C>, former_role(C, op, c, owner, group, w, V.as_lan(w)), role =>
            R.pmap(C, plan<C>, plan<C>, tail => prepend(C, Field{position, q, x, w, V.var(R.c_size(C, op, c)), role}, tail),
              fields(C, op, owner, group, rest, R.c_bind(C, op, c, x, q, w), F.Int63.succ(position))))))

def constructor(~C: Data, ~op: R.check_ops<C>, +c: C, scope: scope, owner: String, group: List<&2, String>, params: P.telescope, value: P.ctor) -> R.program<C, plan<C>>:
  match value:
    case P.Ctor{_, tele, _, _, _}:
      R.pbind(C, C, plan<C>, open(C, op, scope, params, c), fresh => fields(C, op, owner, group, tele, fresh, F.Int63.zero))

def family(~C: Data, ~op: R.check_ops<C>, +c: C, scope: scope, +owner: String, group: List<&2, String>, +key: String, +value: P.family) -> R.program<C, plan<C>>:
  match value:
    case P.Family{_, params, _, _, _, _, _}:
      R.pbind(C, P.ctor, plan<C>, R.crequire(C, P.ctor, P.ctor_of(key, value), E.Unbound{String.concat([key, " is not a constructor of ", owner])}), ct => constructor(C, op, c, scope, owner, group, params, ct))

def layout(~C: Data, ~op: R.check_ops<C>, +c: C, scope: scope, +owner: String, key: String) -> R.program<C, plan<C>>:
  R.pbind(C, P.family, plan<C>, R.mu_family(C, op, c, owner), fam => family(C, op, c, scope, owner, Group.members(R.o_families(C, R.c_runtime(C, op), c), owner), key, fam))

# The IH list is separate from constructor fields. No body-binder order is implied.
def hypotheses(fs: List<&2, field>) -> List<&2, field>:
  match fs:
    case []: []
    case +f <> rest:
      match f:
        case Field{_, _, _, _, _, Ordinary{}}: hypotheses(rest)
        case Field{_, _, _, _, _, Direct{_}}: f <> hypotheses(rest)

def arity(-C: Data, value: plan<C>) -> F.Int63.t:
  match value:
    case Plan{_, +fs}: F.Int63.add(R.length(field, fs), R.length(field, hypotheses(fs)))
