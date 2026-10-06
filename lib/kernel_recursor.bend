# Direct recursive-field layouts. This is metadata, not a recursor certificate.
import Base
import ./foundation.bend as F
import ./kernel_check.bend as C
import ./kernel_error.bend as E
import ./kernel_eval.bend as EV
import ./kernel_global.bend as G
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
type plan is Data:
  Plan{context: C.ctx, fields: List<&2, field>}

def rules(-A: Data, +c: C.ctx, value: R.program<C.ctx, A>) -> R.result(A):
  C.result_of(A, C.drive(A, value, C.budget(c)))

# The declaration parameters open as abstract locals, or as locals defined at the parameter values of a
# scrutinee type. A defined parameter is not a hypothesis of the motive or the bodies: its quantity is Zero.
type scope is Data:
  Abstract{}
  Instance{values: List<&2, V.t>}

def abstract(tele: P.telescope, +c: C.ctx) -> R.result(C.ctx):
  match tele:
    case []: Done{c}
    case F.Pair2{q, F.Pair2{x, ty}} <> rest:
      R.bind(V.t, C.ctx, EV.eval(C.globals(c), C.env(c), ty), tyv => abstract(rest, C.bind(x, q, tyv, c)))

def instance(tele: P.telescope, values: List<&2, V.t>, +c: C.ctx) -> R.result(C.ctx):
  match tele, values:
    case [], []: Done{c}
    case [], _ <> _: Fail{E.Mismatch{R.wrong_pack}}
    case _ <> _, []: Fail{E.Mismatch{R.wrong_pack}}
    case F.Pair2{_, F.Pair2{x, ty}} <> rest, v <> tail:
      R.bind(V.t, C.ctx, EV.eval(C.globals(c), C.env(c), ty), tyv => instance(rest, tail, C.define(x, Q.Zero{}, tyv, v, c)))

def open(scope: scope, tele: P.telescope, +c: C.ctx) -> R.result(C.ctx):
  match scope:
    case Abstract{}: abstract(tele, c)
    case Instance{values}: instance(tele, values, c)

def parameters(tele: P.telescope, +c: C.ctx) -> R.result(C.ctx): open(Abstract{}, tele, c)

def install(scope: scope, x: String, q: Q.t, tyv: V.t, v: V.t, +c: C.ctx) -> C.ctx:
  match scope:
    case Abstract{}: C.bind(x, q, tyv, c)
    case Instance{_}: C.define(x, Q.Zero{}, tyv, v, c)

def refusal(+owner: String, tm: T.t) -> E.t:
  E.Not_yet{F.choose(String, P.occurs([owner], tm), "recursor layout supports direct recursive fields only", "recursor layout does not support mutual families")}

def ordinary(+c: C.ctx, +owner: String, group: List<&2, String>, ty: V.t) -> R.result(role):
  R.bind(T.t, role, EV.quote(C.globals(c), C.size(c), ty), +tm =>
    Q.if_else(R.result(role), P.occurs(group, tm),
      _ => Fail{refusal(owner, tm)},
      _ => Done{Ordinary{}}))

def family_role(+c: C.ctx, +owner: String, +group: List<&2, String>, +ty: V.t, view: Maybe<&2, F.Pair2<String, List<&2, V.t>>>) -> R.result(role):
  match view:
    case None{}: ordinary(c, owner, group, ty)
    case Some{F.Pair2{name, indices}}:
      Q.if_else(R.result(role), String.eq(owner, name), _ => Done{Direct{indices}}, _ => ordinary(c, owner, group, ty))

def former_role(+c: C.ctx, +owner: String, +group: List<&2, String>, +ty: V.t, view: Maybe<&2, V.Former>) -> R.result(role):
  match view:
    case None{}: ordinary(c, owner, group, ty)
    case Some{V.Former{shape, _, _}}: family_role(c, owner, group, ty, R.as_mu(V.t, shape))

def prepend(field: field, value: plan) -> plan:
  match value:
    case Plan{c, tail}: Plan{c, field <> tail}

def fields(+owner: String, +group: List<&2, String>, tele: P.telescope, +c: C.ctx, +position: F.Int63.t) -> R.result(plan):
  match tele:
    case []: Done{Plan{c, []}}
    case F.Pair2{+q, F.Pair2{+x, ty}} <> rest:
      R.bind(V.t, plan, EV.eval(C.globals(c), C.env(c), ty), tyv =>
        R.bind(V.t, plan, EV.whnf(C.globals(c), tyv), +w =>
          R.bind(role, plan, former_role(c, owner, group, w, V.as_lan(w)), role =>
            R.map_result(plan, plan, tail => prepend(Field{position, q, x, w, V.var(C.size(c)), role}, tail),
              fields(owner, group, rest, C.bind(x, q, w, c), F.Int63.succ(position))))))

def constructor(+c: C.ctx, scope: scope, owner: String, group: List<&2, String>, params: P.telescope, value: P.ctor) -> R.result(plan):
  match value:
    case P.Ctor{_, tele, _, _, _}:
      R.bind(C.ctx, plan, open(scope, params, c), fresh => fields(owner, group, tele, fresh, F.Int63.zero))

def family(+c: C.ctx, scope: scope, +owner: String, group: List<&2, String>, +key: String, +value: P.family) -> R.result(plan):
  match value:
    case P.Family{_, params, _, _, _, _, _}:
      R.bind(P.ctor, plan, R.require(P.ctor, P.ctor_of(key, value), E.Unbound{String.concat([key, " is not a constructor of ", owner])}), ct => constructor(c, scope, owner, group, params, ct))

def layout(+c: C.ctx, scope: scope, +owner: String, key: String) -> R.result(plan):
  R.bind(P.family, plan, rules(P.family, c, R.mu_family(C.ctx, C.ops, c, owner)), fam => family(c, scope, owner, Group.members(G.family_list(C.globals(c)), owner), key, fam))

# The IH list is separate from constructor fields. No body-binder order is implied.
def hypotheses(fs: List<&2, field>) -> List<&2, field>:
  match fs:
    case []: []
    case +f <> rest:
      match f:
        case Field{_, _, _, _, _, Ordinary{}}: hypotheses(rest)
        case Field{_, _, _, _, _, Direct{_}}: f <> hypotheses(rest)

def arity(value: plan) -> F.Int63.t:
  match value:
    case Plan{_, +fs}: F.Int63.add(R.length(field, fs), R.length(field, hypotheses(fs)))
