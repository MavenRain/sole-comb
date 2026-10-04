# Direct recursive-field layouts. This is metadata, not a recursor certificate.
import Base
import ./foundation.bend as F
import ./kernel_check.bend as C
import ./kernel_error.bend as E
import ./kernel_eval.bend as EV
import ./kernel_global.bend as G
import ./kernel_positivity.bend as P
import ./kernel_quantity.bend as Q
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

def parameters(tele: P.telescope, +c: C.ctx) -> R.result(C.ctx):
  match tele:
    case []: Done{c}
    case F.Pair2{q, F.Pair2{x, ty}} <> rest:
      R.bind(V.t, C.ctx, EV.eval(C.globals(c), C.env(c), ty), tyv => parameters(rest, C.bind(x, q, tyv, c)))

# The group holds the families whose constructor fields reach the owner. Families are
# declared in order, so an owner field can mention only the owner and its mutual siblings.
def reaches(+names: List<&2, String>, ctors: List<&2, P.ctor>) -> Bool:
  match ctors:
    case []: False{}
    case P.Ctor{_, args, _, _, _} <> rest: Bool.or(P.self_rec(names, args), reaches(names, rest))

def admit(yes: Bool, n: String, +tail: List<&2, String>) -> List<&2, String>:
  F.choose(List<&2, String>, yes, n <> tail, tail)

def joining(+names: List<&2, String>, fams: List<&2, F.Pair2<String, P.family>>) -> List<&2, String>:
  match fams:
    case []: []
    case F.Pair2{+n, P.Family{_, _, _, _, _, ctors, _}} <> rest:
      admit(Bool.and(Bool.not(T.member(n, names)), reaches(names, ctors)), n, joining(names, rest))

def extend(+names: List<&2, String>, added: List<&2, String>, next: List<&2, String> -> List<&2, String>) -> List<&2, String>:
  match added:
    case []: names
    case n <> more: next(List.append(&2, String, n <> more, names))

# A round that adds no family is the fixpoint. Every other round adds one or more families,
# so one round of fuel for each declared family is sufficient.
def reach(fuel: List<&2, F.Pair2<String, P.family>>, +fams: List<&2, F.Pair2<String, P.family>>, +names: List<&2, String>) -> List<&2, String>:
  match fuel:
    case []: names
    case _ <> rest: extend(names, joining(names, fams), grown => reach(rest, fams, grown))

def members(+fams: List<&2, F.Pair2<String, P.family>>, owner: String) -> List<&2, String>:
  reach(fams, fams, [owner])

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

def constructor(+c: C.ctx, owner: String, group: List<&2, String>, params: P.telescope, value: P.ctor) -> R.result(plan):
  match value:
    case P.Ctor{_, tele, _, _, _}:
      R.bind(C.ctx, plan, parameters(params, C.make(C.globals(c), C.budget(c))), fresh => fields(owner, group, tele, fresh, F.Int63.zero))

def family(+c: C.ctx, +owner: String, group: List<&2, String>, +key: String, +value: P.family) -> R.result(plan):
  match value:
    case P.Family{_, params, _, _, _, _, _}:
      R.bind(P.ctor, plan, R.require(P.ctor, P.ctor_of(key, value), E.Unbound{String.concat([key, " is not a constructor of ", owner])}), ct => constructor(c, owner, group, params, ct))

def layout(+c: C.ctx, +owner: String, key: String) -> R.result(plan):
  R.bind(P.family, plan, rules(P.family, c, R.mu_family(C.ctx, C.ops, c, owner)), fam => family(c, owner, members(G.family_list(C.globals(c)), owner), key, fam))

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
