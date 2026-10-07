# Checked branch types for direct structural recursor layouts.
import Base
import ./foundation.bend as F
import ./kernel_error.bend as E
import ./kernel_level.bend as L
import ./kernel_positivity.bend as P
import ./kernel_quantity.bend as Q
import ./kernel_recursor.bend as Rec
import ./kernel_recursor_motive.bend as Motive
import ./kernel_rules.bend as R
import ./kernel_shape.bend as S
import ./kernel_term.bend as T
import ./kernel_value.bend as V

type kind is Data:
  FieldBinder{}
  HypothesisBinder{child: F.Int63.t}
type binder is Data:
  Binder{position: F.Int63.t, quantity: Q.t, name: String, domain: V.t, kind: kind}
type plan<-C: Data> is Data:
  Plan{motive: Motive.plan<C>, context: C, binders: List<&2, binder>, target: V.t}

# Computing a hypothesis consumes its child. An affine child cannot also be used in the body.
def grade(q: Q.t) -> R.result(Q.t):
  match q:
    case Q.Zero{}: Done{Q.Zero{}}
    case Q.One{}: Fail{E.Not_yet{"recursor hypotheses for affine recursive fields are not supported"}}
    case Q.Many{}: Done{Q.Many{}}

def fields(fs: List<&2, Rec.field>) -> List<&2, binder>:
  match fs:
    case []: []
    case Rec.Field{position, q, name, ty, _, _} <> rest: Binder{position, q, name, ty, FieldBinder{}} <> fields(rest)

def values(fs: List<&2, Rec.field>) -> List<&2, V.t>:
  match fs:
    case []: []
    case Rec.Field{_, _, _, _, value, _} <> rest: value <> values(rest)

def child(+position: F.Int63.t, fs: List<&2, Rec.field>) -> R.result(Q.t):
  match fs:
    case []: Fail{E.Mismatch{"a recursor hypothesis has no recursive field"}}
    case Rec.Field{p, q, _, _, _, _} <> rest:
      Q.if_else(R.result(Q.t), F.Int63.equal(p, position), _ => grade(q), _ => child(position, rest))

# Hypothesis binders follow every field. The k-th hypothesis takes the k-th direct child.
def hypotheses(hs: List<&2, Motive.hypothesis>, +fs: List<&2, Rec.field>, +count: F.Int63.t) -> R.result(List<&2, binder>):
  match hs:
    case []: Done{[]}
    case Motive.Hypothesis{+position, name, ty} <> rest:
      R.bind(Q.t, List<&2, binder>, child(position, fs), q =>
        R.map_result(List<&2, binder>, List<&2, binder>, tail => Binder{count, q, name, ty, HypothesisBinder{position}} <> tail, hypotheses(rest, fs, F.Int63.succ(count))))

# The ordinary elimination target: the motive at the declared indices and the constructor
# value. Indices use the parameter and field environment. The motive uses the parameters.
def target(~C: Data, ~op: R.check_ops<C>, +origin: C, +owner: String, +key: String, mo: T.motive, ct: P.ctor, +c: C, fs: List<&2, Rec.field>) -> R.program<C, V.t>:
  R.pbind(C, List<&2, V.t>, V.t, R.mu_values(C, op, c, R.o_env(C, R.c_runtime(C, op), c), R.ctor_indices(ct)), +idx =>
    R.mu_result(C, op, origin, mo, idx, V.VIn{S.SMu{owner, idx}, V.VACtor{key}, values(fs)}))

def formation(~C: Data, ~op: R.check_ops<C>, +c: C, value: V.t) -> R.program<C, Unit>:
  R.pbind(C, T.t, Unit, R.lift(C, T.t, R.o_quote(C, R.c_runtime(C, op), c, value)), tm =>
    R.pmap(C, L.t, Unit, _ => Unit{}, R.c_univ(C, op, c, tm)))

# Each domain is checked in its own prefix: parameters, earlier fields, earlier hypotheses.
# The target is checked after every binder. The result is the complete branch context.
def certify(~C: Data, ~op: R.check_ops<C>, bs: List<&2, binder>, +c: C, +ty: V.t) -> R.program<C, C>:
  match bs:
    case []: R.pmap(C, Unit, C, _ => c, formation(C, op, c, ty))
    case Binder{_, q, name, +domain, _} <> rest:
      R.pbind(C, Unit, C, formation(C, op, c, domain), _ => certify(C, op, rest, R.c_bind(C, op, c, name, q, domain), ty))

def finish(~C: Data, ~op: R.check_ops<C>, +origin: C, +value: Motive.plan<C>, +bs: List<&2, binder>, +ty: V.t) -> R.program<C, plan<C>>:
  R.pmap(C, C, plan<C>, scope => Plan{value, scope, bs, ty}, certify(C, op, bs, origin, ty))

def assemble(~C: Data, ~op: R.check_ops<C>, +origin: C, +owner: String, +key: String, +mo: T.motive, ct: P.ctor, +value: Motive.plan<C>) -> R.program<C, plan<C>>:
  match value:
    case Motive.Plan{raw, _, hs}:
      match raw:
        case Rec.Plan{+c, +fs}:
          R.pbind(C, List<&2, binder>, plan<C>, R.lift(C, List<&2, binder>, hypotheses(hs, fs, R.length(Rec.field, fs))), ihs =>
            R.pbind(C, V.t, plan<C>, target(C, op, origin, owner, key, mo, ct, c, fs), ty => finish(C, op, origin, value, List.append(&2, binder, fields(fs), ihs), ty)))

# One branch type in the declaration context. This does not check a branch body,
# bind public elimination hypotheses, or evaluate a recursive call.
def layout(~C: Data, ~op: R.check_ops<C>, +c: C, +scope: Rec.scope, +owner: String, +key: String, +mo: T.motive) -> R.program<C, plan<C>>:
  R.pbind(C, P.family, plan<C>, R.mu_family(C, op, c, owner), +fam =>
    R.pbind(C, Motive.plan<C>, plan<C>, Motive.layout(C, op, c, scope, owner, key, mo), value =>
      R.pbind(C, P.ctor, plan<C>, R.crequire(C, P.ctor, P.ctor_of(key, fam), E.Unbound{String.concat([key, " is not a constructor of ", owner])}), ct =>
        R.pbind(C, C, plan<C>, Rec.open(C, op, scope, R.family_params(fam), c), origin => assemble(C, op, origin, owner, key, mo, ct, value)))))
