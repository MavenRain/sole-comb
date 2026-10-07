# Checked branch bodies for direct structural recursor layouts.
import Base
import ./foundation.bend as F
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
def scope(~C: Data, ~op: R.check_ops<C>, marks: List<&2, F.Pair2<Q.t, String>>, bs: List<&2, Branch.binder>, +c: C) -> R.program<C, C>:
  match marks:
    case []: R.Pure{c}
    case F.Pair2{+marked, +name} <> rest:
      match bs:
        case []: R.Abort{E.Mismatch{"a recursor branch binder has no plan binder"}}
        case Branch.Binder{_, +declared, _, domain, kind} <> tail:
          R.pbind(C, Unit, C, R.binder_quantity(C, "recursor branch binder", source(kind), name, marked, declared), _ =>
            scope(C, op, rest, tail, R.c_bind(C, op, c, name, declared, domain)))

# The body is checked at the runtime mode. The close returns the usage of the parameters only.
def body(~C: Data, ~op: R.check_ops<C>, +origin: C, +mode: Q.t, tm: T.t, target: V.t, +inner: C) -> R.program<C, Q.usage>:
  R.pbind(C, Q.usage, Q.usage, R.c_check(C, op, inner, Q.runtime(mode), tm, target), uses =>
    R.c_close(C, op, inner, R.c_size(C, op, origin), mode, uses))

def arity(+key: String, +got: F.Int63.t, +want: F.Int63.t) -> R.result(Unit):
  Q.if_else(R.result(Unit), F.Int63.equal(got, want), _ => Done{Unit{}}, _ =>
    Fail{E.Missing_branch{String.concat(["the recursor branch at ", key, " binds ", F.Int63.show(got), " binders and ", key, " takes ", F.Int63.show(want), " fields and hypotheses"])}})

def leg(~C: Data, ~op: R.check_ops<C>, +origin: C, +mode: Q.t, +key: String, value: Branch.plan<C>, lg: T.leg) -> R.program<C, Q.usage>:
  match value:
    case Branch.Plan{_, _, +bs, target}:
      match lg:
        case T.Leg{+marks, tm}:
          R.pbind(C, Unit, Q.usage, R.lift(C, Unit, arity(key, R.length(F.Pair2<Q.t, String>, marks), R.length(Branch.binder, bs))), _ =>
            R.pbind(C, C, Q.usage, scope(C, op, marks, bs, origin), inner => body(C, op, origin, mode, tm, target, inner)))

# One branch leg against its plan, under the given context and the declaration
# parameters. This does not check constructor coverage, bind public elimination
# hypotheses, or evaluate a recursive call.
def check(~C: Data, ~op: R.check_ops<C>, +c: C, +scope: Rec.scope, +mode: Q.t, +owner: String, +key: String, +mo: T.motive, lg: T.leg) -> R.program<C, Q.usage>:
  R.pbind(C, Branch.plan<C>, Q.usage, Branch.layout(C, op, c, scope, owner, key, mo), value =>
    R.pbind(C, P.family, Q.usage, R.mu_family(C, op, c, owner), fam =>
      R.pbind(C, C, Q.usage, Rec.open(C, op, scope, R.family_params(fam), c), origin => leg(C, op, origin, mode, key, value, lg))))
