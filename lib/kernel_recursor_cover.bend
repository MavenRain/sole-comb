# Checked constructor coverage for direct structural recursor branches.
import Base
import ./foundation.bend as F
import ./kernel_check.bend as C
import ./kernel_error.bend as E
import ./kernel_positivity.bend as P
import ./kernel_quantity.bend as Q
import ./kernel_recursor.bend as Rec
import ./kernel_recursor_body.bend as Body
import ./kernel_rules.bend as R
import ./kernel_term.bend as T
import ./kernel_value.bend as V

# The motive check needs a constructor key. A family without constructors would leave the motive unchecked.
def inhabited(names: List<&2, String>) -> R.result(List<&2, String>):
  match names:
    case []: Fail{E.Not_yet{"recursor coverage for a family without constructors is not supported"}}
    case name <> rest: Done{name <> rest}

# Bodies follow the declaration order. The usage is the join over every branch.
def walk(names: List<&2, String>, +c: C.ctx, +scope: Rec.scope, +mode: Q.t, +owner: String, +mo: T.motive, +branches: List<&2, F.Pair2<T.addr, T.leg>>) -> R.result(Q.usage):
  match names:
    case []: Done{Q.unreachable}
    case +key <> rest:
      R.bind(T.leg, Q.usage, R.require(T.leg, R.branch_leg(V.VACtor{key}, branches), E.Missing_branch{String.concat(["the elimination of ", owner, " has no branch at ", key])}), lg =>
        R.bind(Q.usage, Q.usage, Body.check(c, scope, mode, owner, key, mo, lg), uses =>
          R.map_result(Q.usage, Q.usage, tail => Q.alternative(uses, tail), walk(rest, c, scope, mode, owner, mo, branches))))

# Coverage is decided before any body: each constructor has one branch and each branch names a constructor.
def check(+c: C.ctx, +scope: Rec.scope, +mode: Q.t, +owner: String, +mo: T.motive, +branches: List<&2, F.Pair2<T.addr, T.leg>>) -> R.result(Q.usage):
  R.bind(P.family, Q.usage, Rec.rules(P.family, c, R.mu_family(C.ctx, C.ops, c, owner)), fam =>
    R.bind(List<&2, String>, Q.usage, Rec.rules(List<&2, String>, c, R.mu_ctor_names(C.ctx, owner, fam)), all =>
      R.bind(List<&2, String>, Q.usage, inhabited(all), +names =>
        R.bind(Unit, Q.usage, Rec.rules(Unit, c, R.mu_cover(C.ctx, owner, names, branches)), _ => walk(names, c, scope, mode, owner, mo, branches)))))
