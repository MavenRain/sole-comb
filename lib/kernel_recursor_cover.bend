# Checked constructor coverage for direct structural recursor branches.
import Base
import ./foundation.bend as F
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
def walk(~C: Data, ~op: R.check_ops<C>, names: List<&2, String>, +c: C, +scope: Rec.scope, +mode: Q.t, +owner: String, +mo: T.motive, +branches: List<&2, F.Pair2<T.addr, T.leg>>) -> R.program<C, Q.usage>:
  match names:
    case []: R.Pure{Q.unreachable}
    case +key <> rest:
      R.pbind(C, T.leg, Q.usage, R.crequire(C, T.leg, R.branch_leg(V.VACtor{key}, branches), E.Missing_branch{String.concat(["the elimination of ", owner, " has no branch at ", key])}), lg =>
        R.pbind(C, Q.usage, Q.usage, Body.check(C, op, c, scope, mode, owner, key, mo, lg), uses =>
          R.pmap(C, Q.usage, Q.usage, tail => Q.alternative(uses, tail), walk(C, op, rest, c, scope, mode, owner, mo, branches))))

# Coverage is decided before any body: each constructor has one branch and each branch names a constructor.
def check(~C: Data, ~op: R.check_ops<C>, +c: C, +scope: Rec.scope, +mode: Q.t, +owner: String, +mo: T.motive, +branches: List<&2, F.Pair2<T.addr, T.leg>>) -> R.program<C, Q.usage>:
  R.pbind(C, P.family, Q.usage, R.mu_family(C, op, c, owner), fam =>
    R.pbind(C, List<&2, String>, Q.usage, R.mu_ctor_names(C, owner, fam), all =>
      R.pbind(C, List<&2, String>, Q.usage, R.lift(C, List<&2, String>, inhabited(all)), +names =>
        R.pbind(C, Unit, Q.usage, R.mu_cover(C, owner, names, branches), _ => walk(C, op, names, c, scope, mode, owner, mo, branches)))))
