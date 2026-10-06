# Checked recursor scrutinee and result type. The kernel Elim term is not routed to this entry.
import Base
import ./foundation.bend as F
import ./kernel_check.bend as C
import ./kernel_error.bend as E
import ./kernel_positivity.bend as P
import ./kernel_quantity.bend as Q
import ./kernel_recursor.bend as Rec
import ./kernel_recursor_cover.bend as Cover
import ./kernel_recursor_motive.bend as Motive
import ./kernel_rules.bend as R
import ./kernel_shape.bend as S
import ./kernel_term.bend as T
import ./kernel_value.bend as V

def view() -> Data: F.Pair2<V.t, F.Pair2<String, List<&2, V.t>>>

# The scrutinee type must be a left former at a family. The view is its weak head form, the family name and the index values.
def former(+origin: C.ctx, ty: V.t) -> R.result(view):
  Rec.rules(view, origin, R.with_former(C.ctx, view, C.ops, origin, False{}, "the scrutinee is not a left former", ty, w => vs => _ =>
    R.pmap(C.ctx, F.Pair2<String, List<&2, V.t>>, view, pair => F.Pair2{w, pair}, R.crequire(C.ctx, F.Pair2<String, List<&2, V.t>>, R.as_mu(V.t, vs), E.Mismatch{R.wrong_pack}))))

def named(+owner: String, +n: String) -> R.result(Unit):
  Q.if_else(R.result(Unit), String.eq(n, owner), _ => Done{Unit{}}, _ => Fail{E.Mismatch{String.concat(["the scrutinee is in the family ", n, " and the recursor eliminates ", owner])}})

# The motive and the bodies are checked at the declaration parameters. One conversion against the owner former there refuses other parameters.
def uniform(+origin: C.ctx, owner: String, fam: P.family, +w: V.t, ixv: List<&2, V.t>) -> R.result(Unit):
  R.bind(Bool, Unit, C.conv_type(origin, w, V.VLan{S.SMu{owner, ixv}, Motive.diagram(R.family_params(fam), origin), R.annotation(w)}), same =>
    Q.if_else(R.result(Unit), same, _ => Done{Unit{}}, _ => Fail{E.Not_yet{"recursor scrutinee parameters other than the declaration parameters are not supported"}}))

# The type is the motive at the inferred indices and the scrutinee value. The scrutinee is used before the branches.
def result(+origin: C.ctx, +mode: Q.t, mo: T.motive, scrut: T.t, ixv: List<&2, V.t>, su: Q.usage, bu: Q.usage) -> R.result(R.inferred):
  Rec.rules(R.inferred, origin, R.pbind(C.ctx, V.t, R.inferred, R.c_eval(C.ctx, C.ops, origin, scrut), sv =>
    R.pmap(C.ctx, V.t, R.inferred, ty => F.Pair2{ty, Q.sequence(su, Q.scale(mode, bu))}, R.mu_result(C.ctx, C.ops, origin, mo, ixv, sv))))

# Coverage and bodies run after the family name check and the parameter check.
def viewed(+c: C.ctx, +mode: Q.t, +owner: String, scrut: T.t, +mo: T.motive, branches: List<&2, F.Pair2<T.addr, T.leg>>, fam: P.family, +origin: C.ctx, su: Q.usage, v: view) -> R.result(R.inferred):
  match v:
    case F.Pair2{w, F.Pair2{n, +ixv}}:
      R.bind(Unit, R.inferred, named(owner, n), _ => R.bind(Unit, R.inferred, uniform(origin, owner, fam, w, ixv), _ =>
          R.bind(Q.usage, R.inferred, Cover.check(c, mode, owner, mo, branches), bu => result(origin, mode, mo, scrut, ixv, su, bu))))

def typed(+c: C.ctx, +mode: Q.t, +owner: String, scrut: T.t, +mo: T.motive, branches: List<&2, F.Pair2<T.addr, T.leg>>, fam: P.family, +origin: C.ctx, inf: R.inferred) -> R.result(R.inferred):
  match inf:
    case F.Pair2{ty, su}: R.bind(view, R.inferred, former(origin, ty), v => viewed(c, mode, owner, scrut, mo, branches, fam, origin, su, v))

# The scope is the given context, then the declaration parameters. This does not instantiate parameters.
def check(+c: C.ctx, +mode: Q.t, +owner: String, +sq: Q.t, +scrut: T.t, +mo: T.motive, +branches: List<&2, F.Pair2<T.addr, T.leg>>) -> R.result(R.inferred):
  R.bind(P.family, R.inferred, Rec.rules(P.family, c, R.mu_family(C.ctx, C.ops, c, owner)), +fam =>
    R.bind(C.ctx, R.inferred, Rec.parameters(R.family_params(fam), c), +origin =>
      R.bind(R.inferred, R.inferred, Rec.rules(R.inferred, origin, R.infer_scrutinee(C.ctx, C.ops, origin, mode, sq, scrut)), inf => typed(c, mode, owner, scrut, mo, branches, fam, origin, inf))))
