# Checked recursor scrutinee at instantiated parameters. The kernel Elim term is not routed to this entry.
import Base
import ./foundation.bend as F
import ./kernel_check.bend as C
import ./kernel_error.bend as E
import ./kernel_positivity.bend as P
import ./kernel_quantity.bend as Q
import ./kernel_recursor.bend as Rec
import ./kernel_recursor_bridge.bend as Bridge
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
def former(+c: C.ctx, ty: V.t) -> R.result(view):
  Bridge.rules(view, c, R.with_former(C.ctx, view, C.ops, c, False{}, "the scrutinee is not a left former", ty, _ => vs => d =>
    R.pmap(C.ctx, F.Pair2<String, List<&2, V.t>>, view, pair => closed(d, pair), R.crequire(C.ctx, F.Pair2<String, List<&2, V.t>>, R.as_mu(V.t, vs), E.Mismatch{R.wrong_pack}))))

def named(+owner: String, +n: String) -> R.result(Unit):
  Q.if_else(R.result(Unit), String.eq(n, owner), _ => Done{Unit{}}, _ => Fail{E.Mismatch{String.concat(["the scrutinee is in the family ", n, " and the recursor eliminates ", owner])}})

# The parameter values of the scrutinee type in telescope order: the first parameter first.
def values(+c: C.ctx, params: P.telescope, d: V.closure) -> R.result(List<&2, V.t>):
  R.map_result(List<&2, V.t>, List<&2, V.t>, env => R.rev_append(V.t, env, []), Bridge.rules(List<&2, V.t>, c, Instance.environment(C.ctx, C.ops, c, params, d)))

# The type is the motive at the inferred indices and the scrutinee value. The scrutinee value is over the given
# context; the motive is over the origin. Usage rows are keyed by level, so the two usages align without padding.
def result(+c: C.ctx, +origin: C.ctx, +mode: Q.t, mo: T.motive, scrut: T.t, ixv: List<&2, V.t>, su: Q.usage, bu: Q.usage) -> R.result(R.inferred):
  Bridge.rules(R.inferred, origin, R.pbind(C.ctx, V.t, R.inferred, R.c_eval(C.ctx, C.ops, c, scrut), sv =>
    R.pmap(C.ctx, V.t, R.inferred, ty => F.Pair2{ty, Q.sequence(su, Q.scale(mode, bu))}, R.mu_result(C.ctx, C.ops, origin, mo, ixv, sv))))

# The origin is the given context, then the declaration parameters defined at the scrutinee values.
# Coverage and bodies run after the family name check.
def scoped(+c: C.ctx, +mode: Q.t, +owner: String, scrut: T.t, +mo: T.motive, branches: List<&2, F.Pair2<T.addr, T.leg>>, fam: P.family, su: Q.usage, ixv: List<&2, V.t>, +scope: Rec.scope) -> R.result(R.inferred):
  R.bind(C.ctx, R.inferred, Bridge.rules(C.ctx, c, Rec.open(C.ctx, C.ops, scope, R.family_params(fam), c)), origin =>
    R.bind(Q.usage, R.inferred, Bridge.rules(Q.usage, c, Cover.check(C.ctx, C.ops, c, scope, mode, owner, mo, branches)), bu => result(c, origin, mode, mo, scrut, ixv, su, bu)))

def viewed(+c: C.ctx, +mode: Q.t, +owner: String, scrut: T.t, +mo: T.motive, branches: List<&2, F.Pair2<T.addr, T.leg>>, +fam: P.family, su: Q.usage, v: view) -> R.result(R.inferred):
  match v:
    case F.Pair2{n, F.Pair2{ixv, d}}:
      R.bind(Unit, R.inferred, named(owner, n), _ =>
        R.bind(List<&2, V.t>, R.inferred, values(c, R.family_params(fam), d), vals => scoped(c, mode, owner, scrut, mo, branches, fam, su, ixv, Rec.Instance{vals})))

def typed(+c: C.ctx, +mode: Q.t, +owner: String, scrut: T.t, +mo: T.motive, branches: List<&2, F.Pair2<T.addr, T.leg>>, fam: P.family, inf: R.inferred) -> R.result(R.inferred):
  match inf:
    case F.Pair2{ty, su}: R.bind(view, R.inferred, former(c, ty), v => viewed(c, mode, owner, scrut, mo, branches, fam, su, v))

# The scrutinee is inferred in the given context. Its type instantiates the declaration parameters.
def check(+c: C.ctx, +mode: Q.t, +owner: String, +sq: Q.t, +scrut: T.t, +mo: T.motive, +branches: List<&2, F.Pair2<T.addr, T.leg>>) -> R.result(R.inferred):
  R.bind(P.family, R.inferred, Bridge.rules(P.family, c, R.mu_family(C.ctx, C.ops, c, owner)), fam =>
    R.bind(R.inferred, R.inferred, Bridge.rules(R.inferred, c, R.infer_scrutinee(C.ctx, C.ops, c, mode, sq, scrut)), inf => typed(c, mode, owner, scrut, mo, branches, fam, inf)))
