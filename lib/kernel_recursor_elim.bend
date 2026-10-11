# Checked recursor scrutinee at instantiated parameters. The kernel Elim term routes to this entry when a leg binds hypotheses.
import Base
import ./foundation.bend as F
import ./kernel_error.bend as E
import ./kernel_positivity.bend as P
import ./kernel_quantity.bend as Q
import ./kernel_recursor.bend as Rec
import ./kernel_recursor_cover.bend as Cover
import ./kernel_recursor_instance.bend as Instance
import ./kernel_rules.bend as R
import ./kernel_term.bend as T
import ./kernel_value.bend as V

# The view of a scrutinee type: the family name, the index values and the former closure.
def view() -> Data: F.Pair2<String, F.Pair2<List<&2, V.t>, V.closure>>

def closed(d: V.closure, pair: F.Pair2<String, List<&2, V.t>>) -> view:
  match pair:
    case F.Pair2{n, ixv}: F.Pair2{n, F.Pair2{ixv, d}}

# The scrutinee type must be a left former at a family. The closure keeps the parameter legs of the type.
def former(~C: Data, ~op: R.check_ops<C>, +c: C, ty: V.t) -> R.program<C, view>:
  R.with_former(C, view, op, c, False{}, "the scrutinee is not a left former", ty, _ => vs => d =>
    R.pmap(C, F.Pair2<String, List<&2, V.t>>, view, pair => closed(d, pair), R.crequire(C, F.Pair2<String, List<&2, V.t>>, R.as_mu(V.t, vs), E.Mismatch{R.wrong_pack})))

def named(+owner: String, +n: String) -> R.result(Unit):
  Q.if_else(R.result(Unit), String.eq(n, owner), _ => Done{Unit{}}, _ => Fail{E.Mismatch{String.concat(["the scrutinee is in the family ", n, " and the recursor eliminates ", owner])}})

# The parameter values of the scrutinee type in telescope order: the first parameter first.
def values(~C: Data, ~op: R.check_ops<C>, +c: C, params: P.telescope, d: V.closure) -> R.program<C, List<&2, V.t>>:
  R.pmap(C, List<&2, V.t>, List<&2, V.t>, env => R.rev_append(V.t, env, []), Instance.environment(C, op, c, params, d))

# The type is the motive at the inferred indices and the scrutinee value. The scrutinee value is over the given
# context; the motive is over the origin. Usage rows are keyed by level, so the two usages align without padding.
# The parameter rows are closed.
def result(~C: Data, ~op: R.check_ops<C>, +c: C, +origin: C, +mode: Q.t, mo: T.motive, scrut: T.t, ixv: List<&2, V.t>, su: Q.usage, bu: Q.usage) -> R.program<C, R.inferred>:
  R.pbind(C, V.t, R.inferred, R.c_eval(C, op, c, scrut), sv =>
    R.pbind(C, Q.usage, R.inferred, R.c_close(C, op, origin, R.c_size(C, op, c), mode, bu), bu2 =>
      R.pmap(C, V.t, R.inferred, ty => F.Pair2{ty, Q.sequence(su, Q.scale(mode, bu2))}, R.mu_result(C, op, origin, mo, ixv, sv))))

# The origin is the given context, then the declaration parameters defined at the scrutinee values.
# Coverage and bodies run after the family name check.
def scoped(~C: Data, ~op: R.check_ops<C>, +c: C, +mode: Q.t, +owner: String, scrut: T.t, +mo: T.motive, branches: List<&2, F.Pair2<T.addr, T.leg>>, fam: P.family, su: Q.usage, ixv: List<&2, V.t>, +scope: Rec.scope) -> R.program<C, R.inferred>:
  R.pbind(C, C, R.inferred, Rec.open(C, op, scope, R.family_params(fam), c), origin =>
    R.pbind(C, Q.usage, R.inferred, Cover.check(C, op, c, scope, mode, owner, mo, branches), bu => result(C, op, c, origin, mode, mo, scrut, ixv, su, bu)))

def viewed(~C: Data, ~op: R.check_ops<C>, +c: C, +mode: Q.t, +owner: String, scrut: T.t, +mo: T.motive, branches: List<&2, F.Pair2<T.addr, T.leg>>, +fam: P.family, su: Q.usage, v: view) -> R.program<C, R.inferred>:
  match v:
    case F.Pair2{n, F.Pair2{ixv, d}}:
      R.pbind(C, Unit, R.inferred, R.lift(C, Unit, named(owner, n)), _ =>
        R.pbind(C, List<&2, V.t>, R.inferred, values(C, op, c, R.family_params(fam), d), vals => scoped(C, op, c, mode, owner, scrut, mo, branches, fam, su, ixv, Rec.Instance{vals})))

def typed(~C: Data, ~op: R.check_ops<C>, +c: C, +mode: Q.t, +owner: String, scrut: T.t, +mo: T.motive, branches: List<&2, F.Pair2<T.addr, T.leg>>, fam: P.family, inf: R.inferred) -> R.program<C, R.inferred>:
  match inf:
    case F.Pair2{ty, su}: R.pbind(C, view, R.inferred, former(C, op, c, ty), v => viewed(C, op, c, mode, owner, scrut, mo, branches, fam, su, v))

# The scrutinee is inferred in the given context. Its type instantiates the declaration parameters.
def check(~C: Data, ~op: R.check_ops<C>, +c: C, +mode: Q.t, +owner: String, +sq: Q.t, +scrut: T.t, +mo: T.motive, +branches: List<&2, F.Pair2<T.addr, T.leg>>) -> R.program<C, R.inferred>:
  R.pbind(C, P.family, R.inferred, R.mu_family(C, op, c, owner), fam =>
    R.pbind(C, R.inferred, R.inferred, R.infer_scrutinee(C, op, c, mode, sq, scrut), inf => typed(C, op, c, mode, owner, scrut, mo, branches, fam, inf)))
