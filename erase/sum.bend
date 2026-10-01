# Finite sum plans from Kanon 69f3be51 lib/erase.ml:tag_intro, case_elim and branch_of.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_check.bend as C
import ../lib/kernel_error.bend as E
import ../lib/kernel_eval.bend as EV
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_rules.bend as R
import ../lib/kernel_shape.bend as S
import ../lib/kernel_term.bend as T
import ../lib/kernel_value.bend as V
import eterm.bend as K
import type.bend as Ty

type collection is Data:
  Collection{width: F.Int63.t, diagram: V.closure}
type payload is Data:
  Payload{ty: V.t, body: T.t}
type tag is Data:
  Tag{tid: K.tid, index: F.Int63.t, payload: Maybe<&2, payload>}
type case_plan is Data:
  Case{ty: V.t, tid: K.tid, legs: List<&2, V.t>}
type branch is Data:
  Branch{quantity: Q.t, name: String, domain: V.t, keep: Bool, target: V.t, body: T.t}
def require(~A: Data, value: Maybe<&2, A>, message: String) -> R.result(A):
  Ty.maybe(A, R.result(A), value, _ => Fail{E.Mismatch{message}}, +x => Done{x})
# Kanon gives introduction and elimination their own refusal texts here; the shared
# texts differ only after a checking failure or when two refusals compete.
def collection_former(former: V.Former) -> R.result(collection):
  match former:
    case V.Former{shape, +diagram, _}:
      R.map_result(F.Int63.t, collection, +n => Collection{n, diagram}, require(F.Int63.t, R.collection_view(shape), "erasure needs a finite collection former"))
def collection_of(+c: C.ctx, ty: V.t) -> R.result(collection):
  R.bind(V.t, collection, EV.whnf(C.globals(c), ty), +w =>
    R.bind(V.Former, collection, require(V.Former, V.as_lan(w), "a sum stands at a type that is not a left former"), collection_former))
def legs_of(c: C.ctx, value: collection) -> R.result(List<&2, V.t>):
  match value:
    case Collection{n, diagram}: Ty.leg_types(c, n, diagram)
def tag_payload(+c: C.ctx, +ty: V.t, +index: F.Int63.t, +body: T.t, +lty: V.t) -> R.result(tag):
  R.bind(Bool, tag, Ty.runtime_ty(c, lty), +keep =>
    R.map_result(K.tid, tag, +tid => Tag{tid, index, F.choose(Maybe<&2, payload>, keep, Some{Payload{lty, body}}, None{})}, Ty.tid_of(c, ty)))
def injection(+c: C.ctx, +ty: V.t, address: T.addr, +args: List<&2, T.t>) -> R.result(tag):
  R.bind(F.Int63.t, tag, require(F.Int63.t, T.as_aleg(address), "an injection takes a leg address"), +index =>
    R.bind(collection, tag, collection_of(c, ty), +coll =>
      R.bind(List<&2, V.t>, tag, legs_of(c, coll), +tys =>
        R.bind(V.t, tag, require(V.t, R.at(V.t, index, tys), "the leg address is outside the collection"), +lty =>
          R.bind(T.t, tag, require(T.t, R.one_of(T.t, args), "an injection takes one payload"), +body => tag_payload(c, ty, index, body, lty))))))
def case_type(+c: C.ctx, +ty: V.t) -> R.result(case_plan):
  R.bind(collection, case_plan, collection_of(c, ty), +coll =>
    R.bind(List<&2, V.t>, case_plan, legs_of(c, coll), +tys =>
      R.map_result(K.tid, case_plan, +tid => Case{ty, tid, tys}, Ty.tid_of(c, ty))))
# case_of, injection and tag_payload compute the layout and tid before erasure;
# Kanon erases first. It shows only after a checking failure or when two refusals compete.
def case_of(+c: C.ctx, scrut: T.t) -> R.result(case_plan):
  R.bind(V.t, case_plan, C.infer(c, Q.Many{}, scrut), +ty => case_type(c, ty))
@unsafe
def find_branch(+index: F.Int63.t, branches: List<&2, F.Pair2<T.addr, T.leg>>) -> R.result(T.leg):
  match branches:
    case []: Fail{E.Mismatch{"a case is missing a branch"}}
    case F.Pair2{address, +leg} <> +rest:
      Ty.maybe(F.Int63.t, R.result(T.leg), T.as_aleg(address), _ => find_branch(index, rest),
        +k => Ty.boolean(R.result(T.leg), F.Int63.equal(k, index), _ => Done{leg}, _ => find_branch(index, rest)))
def branch_binder(+c: C.ctx, +ty: V.t, +motive: Maybe<&2, T.motive>, +tys: List<&2, V.t>, +index: F.Int63.t, +lty: V.t, +body: T.t, binder: F.Pair2<Q.t, String>) -> R.result(branch):
  match binder:
    case F.Pair2{+q, +name}:
      R.bind(Bool, branch, Ty.point_runtime(c, q, lty), +keep =>
        R.map_result(V.t, branch, +target => Branch{q, name, lty, keep, target, body},
          C.result_of(V.t, C.drive(V.t, R.elim_result(C.ctx, C.ops, c, motive, Some{ty}, V.VIn{S.SColl{Ty.count(V.t, tys)}, V.VALeg{index}, [V.var(C.size(c))]}), C.budget(c)))))
def branch_leg(c: C.ctx, ty: V.t, motive: Maybe<&2, T.motive>, tys: List<&2, V.t>, index: F.Int63.t, lty: V.t, leg: T.leg) -> R.result(branch):
  match leg:
    case T.Leg{binders, +body}:
      R.bind(F.Pair2<Q.t, String>, branch, require(F.Pair2<Q.t, String>, R.one_of(F.Pair2<Q.t, String>, binders), "a case branch takes one binder"), binder => branch_binder(c, ty, motive, tys, index, lty, body, binder))
def branch_of(+c: C.ctx, +ty: V.t, +motive: Maybe<&2, T.motive>, +tys: List<&2, V.t>, +index: F.Int63.t, branches: List<&2, F.Pair2<T.addr, T.leg>>) -> R.result(branch):
  R.bind(V.t, branch, require(V.t, R.at(V.t, index, tys), "a leg number is outside the collection"), +lty =>
    R.bind(T.leg, branch, find_branch(index, branches), leg => branch_leg(c, ty, motive, tys, index, lty, leg)))
