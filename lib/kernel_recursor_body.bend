# Checked branch bodies for direct structural recursor layouts.
import Base
import ./foundation.bend as F
import ./kernel_check.bend as C
import ./kernel_error.bend as E
import ./kernel_positivity.bend as P
import ./kernel_quantity.bend as Q
import ./kernel_recursor.bend as Rec
import ./kernel_recursor_branch.bend as Branch
import ./kernel_rules.bend as R
import ./kernel_term.bend as T
import ./kernel_value.bend as V

def source(k: Branch.kind) -> String:
  match k:
    case Branch.FieldBinder{}: "field"
    case Branch.HypothesisBinder{_}: "hypothesis"

# The leg gives each name. The plan gives each quantity and domain, so the levels agree with the plan target.
def scope(marks: List<&2, F.Pair2<Q.t, String>>, bs: List<&2, Branch.binder>, +c: C.ctx) -> R.result(C.ctx):
  match marks:
    case []: Done{c}
    case F.Pair2{+marked, +name} <> rest:
      match bs:
        case []: Fail{E.Mismatch{"a recursor branch binder has no plan binder"}}
        case Branch.Binder{_, +declared, _, domain, kind} <> tail:
          R.bind(Unit, C.ctx, Rec.rules(Unit, c, R.binder_quantity(C.ctx, "recursor branch binder", source(kind), name, marked, declared)), _ =>
            scope(rest, tail, C.bind(name, declared, domain, c)))

# The body is checked at the runtime mode. The close returns the usage of the parameters only.
def body(+origin: C.ctx, +mode: Q.t, tm: T.t, target: V.t, +inner: C.ctx) -> R.result(Q.usage):
  Rec.rules(Q.usage, inner, R.pbind(C.ctx, Q.usage, Q.usage, R.c_check(C.ctx, C.ops, inner, Q.runtime(mode), tm, target), uses =>
    R.c_close(C.ctx, C.ops, inner, C.size(origin), mode, uses)))

def arity(+key: String, +got: F.Int63.t, +want: F.Int63.t) -> R.result(Unit):
  Q.if_else(R.result(Unit), F.Int63.equal(got, want), _ => Done{Unit{}}, _ =>
    Fail{E.Missing_branch{String.concat(["the recursor branch at ", key, " binds ", F.Int63.show(got), " binders and ", key, " takes ", F.Int63.show(want), " fields and hypotheses"])}})

def leg(+origin: C.ctx, +mode: Q.t, +key: String, value: Branch.plan, lg: T.leg) -> R.result(Q.usage):
  match value:
    case Branch.Plan{_, _, +bs, target}:
      match lg:
        case T.Leg{+marks, tm}:
          R.bind(Unit, Q.usage, arity(key, R.length(F.Pair2<Q.t, String>, marks), R.length(Branch.binder, bs)), _ =>
            R.bind(C.ctx, Q.usage, scope(marks, bs, origin), inner => body(origin, mode, tm, target, inner)))

# One branch leg against its plan, under the given context and the declaration
# parameters. This does not check constructor coverage, bind public elimination
# hypotheses, or evaluate a recursive call.
def check(+c: C.ctx, +scope: Rec.scope, +mode: Q.t, +owner: String, +key: String, +mo: T.motive, lg: T.leg) -> R.result(Q.usage):
  R.bind(Branch.plan, Q.usage, Branch.layout(c, scope, owner, key, mo), value =>
    R.bind(P.family, Q.usage, Rec.rules(P.family, c, R.mu_family(C.ctx, C.ops, c, owner)), fam =>
      R.bind(C.ctx, Q.usage, Rec.open(scope, R.family_params(fam), c), origin => leg(origin, mode, key, value, lg))))
