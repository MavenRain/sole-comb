# Dependent-pair plans from Kanon 69f3be51 lib/erase.ml: pair_intro and pair_elim.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_check.bend as C
import ../lib/kernel_error.bend as E
import ../lib/kernel_eval.bend as EV
import ../lib/kernel_global.bend as G
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_rules.bend as R
import ../lib/kernel_shape.bend as S
import ../lib/kernel_term.bend as T
import ../lib/kernel_value.bend as V
import eterm.bend as K
import type.bend as Ty

type field is Data:
  Field{ty: V.t, term: T.t}
type branch is Data:
  Branch{scrut_type: V.t, tid: K.tid, quantity: Q.t, first_name: String, domain: V.t, keep_first: Bool, second_name: String, fibre: V.t, keep_second: Bool, target: V.t, body: T.t}
def require(~A: Data, value: Maybe<&2, A>, message: String) -> R.result(A):
  Ty.maybe(A, R.result(A), value, _ => Fail{E.Mismatch{message}}, +x => Done{x})
# Only unchecked direct-contract terms reach these texts; Kanon's in_typed/elim_typed texts and point-first order differ.
def point_of(+c: C.ctx, ty: V.t) -> R.result(Ty.arrow):
  R.bind(V.t, Ty.arrow, EV.whnf(C.globals(c), ty), w =>
    R.bind(V.Former, Ty.arrow, require(V.Former, V.as_lan(w), "a pair stands at a type that is not a left former"), former =>
      require(Ty.arrow, Ty.former_arrow(former), "a pair needs a point former")))
def fields(+c: C.ctx, point: Ty.arrow, +p: T.t, fibre: T.t) -> R.result(List<&2, field>):
  match point:
    case Ty.Arrow{+q, x, +dom, +d}:
      R.bind(Bool, List<&2, field>, Ty.point_runtime(c, q, dom), +keep1 =>
        R.bind(F.Pair2<C.ctx, V.t>, List<&2, field>, Ty.under(c, q, x, dom, d), value =>
          Ty.pair(C.ctx, V.t, R.result(List<&2, field>), value, c1 => cod =>
            R.bind(Bool, List<&2, field>, Ty.runtime_ty(c1, cod), +keep2 =>
              R.bind(V.t, List<&2, field>, EV.eval(C.globals(c), C.env(c), p), pv =>
                R.map_result(V.t, List<&2, field>, +fty =>
                  List.append(&2, field, F.choose(List<&2, field>, keep1, [Field{dom, p}], []), F.choose(List<&2, field>, keep2, [Field{fty, fibre}], [])), Ty.opened(c, d, [pv])))))))
def introduction(+c: C.ctx, ty: V.t, address: T.addr, +args: List<&2, T.t>) -> R.result(List<&2, field>):
  R.bind(F.Pair2<Q.t, T.t>, List<&2, field>, require(F.Pair2<Q.t, T.t>, T.as_apt(address), "a pair takes a point address"), value =>
    Ty.pair(Q.t, T.t, R.result(List<&2, field>), value, _ => p =>
      R.bind(T.t, List<&2, field>, require(T.t, R.one_of(T.t, args), "a pair takes one fibre"), fibre =>
        R.bind(Ty.arrow, List<&2, field>, point_of(c, ty), point => fields(c, point, p, fibre)))))
def mapped_shape(+c: C.ctx, shape: S.t<T.t>) -> R.result(S.t<V.t>):
  R.run(G.t, EV.dispatch, S.t<V.t>, EV.eval_shape(C.globals(c), C.env(c), shape))
def branch_target(+c: C.ctx, +ty: V.t, shape: S.t<T.t>, motive: Maybe<&2, T.motive>, q: Q.t) -> R.result(V.t):
  R.bind(S.t<V.t>, V.t, mapped_shape(c, shape), sv =>
    C.result_of(V.t, C.drive(V.t, R.elim_result(C.ctx, C.ops, c, motive, Some{ty}, V.VIn{sv, V.VAPt{q, V.var(C.size(c))}, [V.var(F.Int63.succ(C.size(c)))]}), C.budget(c))))
def branch_point(+c: C.ctx, +sty: V.t, ty: V.t, +shape: S.t<T.t>, +motive: Maybe<&2, T.motive>, +body: T.t, +second: String, point: Ty.arrow) -> R.result(branch):
  match point:
    case Ty.Arrow{+q, +x, +dom, d}:
      R.bind(Bool, branch, Ty.point_runtime(c, q, dom), +keep1 =>
        R.bind(F.Pair2<C.ctx, V.t>, branch, Ty.under(c, q, x, dom, d), value =>
          Ty.pair(C.ctx, V.t, R.result(branch), value, c1 => +cod =>
            R.bind(Bool, branch, Ty.runtime_ty(c1, cod), +keep2 =>
              R.bind(V.t, branch, branch_target(c, ty, shape, motive, q), +target =>
                R.map_result(K.tid, branch, +tid => Branch{sty, tid, q, x, dom, keep1, second, cod, keep2, target, body}, Ty.tid_of(c, sty)))))))
def branch_leg(c: C.ctx, sty: V.t, ty: V.t, shape: S.t<T.t>, motive: Maybe<&2, T.motive>, leg: T.leg, point: Ty.arrow) -> R.result(branch):
  match leg:
    case T.Leg{binders, body}:
      R.bind(F.Pair2<F.Pair2<Q.t, String>, F.Pair2<Q.t, String>>, branch, require(F.Pair2<F.Pair2<Q.t, String>, F.Pair2<Q.t, String>>, R.two_of(F.Pair2<Q.t, String>, binders), "a pair branch takes two binders"), value =>
        Ty.pair(F.Pair2<Q.t, String>, F.Pair2<Q.t, String>, R.result(branch), value, _ => second =>
          Ty.pair(Q.t, String, R.result(branch), second, _ => name => branch_point(c, sty, ty, shape, motive, body, name, point))))
def elimination(+c: C.ctx, +ty: V.t, +shape: S.t<T.t>, scrut: T.t, +motive: Maybe<&2, T.motive>, +branches: List<&2, F.Pair2<T.addr, T.leg>>) -> R.result(branch):
  R.bind(V.t, branch, C.infer(c, Q.Many{}, scrut), +sty =>
    R.bind(Ty.arrow, branch, point_of(c, sty), point =>
      R.bind(F.Pair2<T.addr, T.leg>, branch, require(F.Pair2<T.addr, T.leg>, R.one_of(F.Pair2<T.addr, T.leg>, branches), "a pair elimination takes one branch"), value =>
        Ty.pair(T.addr, T.leg, R.result(branch), value, _ => leg => branch_leg(c, sty, ty, shape, motive, leg, point)))))
