# Checked branch types for direct structural recursor layouts.
import Base
import ./foundation.bend as F
import ./kernel_check.bend as C
import ./kernel_error.bend as E
import ./kernel_eval.bend as EV
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
type plan is Data:
  Plan{motive: Motive.plan, context: C.ctx, binders: List<&2, binder>, target: V.t}

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
def target(+origin: C.ctx, +owner: String, +key: String, mo: T.motive, ct: P.ctor, +c: C.ctx, fs: List<&2, Rec.field>) -> R.result(V.t):
  R.bind(List<&2, V.t>, V.t, Rec.rules(List<&2, V.t>, c, R.mu_values(C.ctx, C.ops, c, C.env(c), R.ctor_indices(ct))), +idx =>
    Rec.rules(V.t, origin, R.mu_result(C.ctx, C.ops, origin, mo, idx, V.VIn{S.SMu{owner, idx}, V.VACtor{key}, values(fs)})))

def formation(+c: C.ctx, value: V.t) -> R.result(Unit):
  R.bind(T.t, Unit, EV.quote(C.globals(c), C.size(c), value), tm =>
    R.map_result(L.t, Unit, _ => Unit{}, Rec.rules(L.t, c, R.c_univ(C.ctx, C.ops, c, tm))))

# Each domain is checked in its own prefix: parameters, earlier fields, earlier hypotheses.
# The target is checked after every binder. The result is the complete branch context.
def certify(bs: List<&2, binder>, +c: C.ctx, +ty: V.t) -> R.result(C.ctx):
  match bs:
    case []: R.map_result(Unit, C.ctx, _ => c, formation(c, ty))
    case Binder{_, q, name, +domain, _} <> rest:
      R.bind(Unit, C.ctx, formation(c, domain), _ => certify(rest, C.bind(name, q, domain, c), ty))

def finish(+origin: C.ctx, +value: Motive.plan, +bs: List<&2, binder>, +ty: V.t) -> R.result(plan):
  R.map_result(C.ctx, plan, scope => Plan{value, scope, bs, ty}, certify(bs, origin, ty))

def assemble(+origin: C.ctx, +owner: String, +key: String, +mo: T.motive, ct: P.ctor, +value: Motive.plan) -> R.result(plan):
  match value:
    case Motive.Plan{raw, _, hs}:
      match raw:
        case Rec.Plan{+c, +fs}:
          R.bind(List<&2, binder>, plan, hypotheses(hs, fs, R.length(Rec.field, fs)), ihs =>
            R.bind(V.t, plan, target(origin, owner, key, mo, ct, c, fs), ty => finish(origin, value, List.append(&2, binder, fields(fs), ihs), ty)))

# One branch type in the declaration context. This does not check a branch body,
# bind public elimination hypotheses, or evaluate a recursive call.
def layout(+c: C.ctx, +scope: Rec.scope, +owner: String, +key: String, +mo: T.motive) -> R.result(plan):
  R.bind(P.family, plan, Rec.rules(P.family, c, R.mu_family(C.ctx, C.ops, c, owner)), +fam =>
    R.bind(Motive.plan, plan, Motive.layout(c, scope, owner, key, mo), value =>
      R.bind(P.ctor, plan, R.require(P.ctor, P.ctor_of(key, fam), E.Unbound{String.concat([key, " is not a constructor of ", owner])}), ct =>
        R.bind(C.ctx, plan, Rec.open(scope, R.family_params(fam), c), origin => assemble(origin, owner, key, mo, ct, value)))))
