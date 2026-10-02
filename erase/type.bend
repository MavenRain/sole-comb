# Type-directed runtime admission and function layouts from Kanon 69f3be51.
# Collection products omit erased fields; sums retain every tag and layout leg.
# Dependent pairs omit erased components; recursive families have nominal layouts.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_budget.bend as B
import ../lib/kernel_check.bend as C
import ../lib/kernel_error.bend as E
import ../lib/kernel_eval.bend as EV
import ../lib/kernel_global.bend as G
import ../lib/kernel_level.bend as L
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_rules.bend as R
import ../lib/kernel_term.bend as T
import ../lib/kernel_value.bend as V
import eterm.bend as K

# Named eliminators keep every match on a Bend parameter or field.
def inspect(~A: Data, -Out: Type, value: A, next: A -> Out) -> Out: next(value)
def maybe(~A: Data, -Out: Type, value: Maybe<&2, A>, none: Unit -> Out, some: A -> Out) -> Out:
  match value:
    case None{}: none(Unit{})
    case Some{x}: some(x)
def boolean(-Out: Type, value: Bool, yes: Unit -> Out, no: Unit -> Out) -> Out:
  match value:
    case True{}: yes(Unit{})
    case False{}: no(Unit{})
def pair(~A: Data, ~B: Data, -Out: Type, value: F.Pair2<A, B>, next: A -> B -> Out) -> Out:
  match value:
    case F.Pair2{a, b}: next(a, b)
def has(~A: Data, value: Maybe<&2, A>) -> Bool:
  match value:
    case None{}: False{}
    case Some{_}: True{}
def empty(~A: Data, values: List<&2, A>) -> Bool:
  match values:
    case []: True{}
    case _ <> _: False{}
type arrow is Data:
  Arrow{quantity: Q.t, name: String, domain: V.t, diagram: V.closure}
def empty_collection(value: Maybe<&2, F.Int63.t>) -> Bool:
  match value:
    case None{}: False{}
    case Some{n}: F.Int63.equal(n, F.Int63.zero)
def proof_free(+c: C.ctx, ty: V.t) -> R.result(Bool):
  R.bind(T.t, Bool, EV.quote(C.globals(c), C.size(c), ty), +tm =>
    R.map_result(L.t, Bool, +l => Bool.not(L.equal(l, L.zero)), C.infer_univ(c, tm)))
def opened(+c: C.ctx, diagram: V.closure, args: List<&2, V.t>) -> R.result(V.t):
  R.run(G.t, EV.dispatch, V.t, R.open_closure(G.t, EV.ev(C.globals(c)), diagram, args))
def any_repr() -> K.repr: K.RUnion{K.Tid{"any"}}
def mu_tid(name: String) -> K.tid: K.Tid{String.concat(["mu<", name, ">"])}
type signature is Data:
  Signature{params: List<&2, K.repr>, result: K.repr}
def point_arrow(diagram: V.closure, value: Maybe<&2, R.Point>) -> Maybe<&2, arrow>:
  match value:
    case None{}: None{}
    case Some{R.Point{q, x, dom}}: Some{Arrow{q, x, dom, diagram}}
def empty_former(former: Maybe<&2, V.Former>) -> Bool:
  match former:
    case None{}: False{}
    case Some{V.Former{shape, _, _}}: empty_collection(R.collection_view(shape))
def under(+c: C.ctx, q: Q.t, x: String, +dom: V.t, d: V.closure) -> R.result(F.Pair2<C.ctx, V.t>):
  R.map_result(V.t, F.Pair2<C.ctx, V.t>, +v => F.Pair2{C.bind(x, q, dom, c), v}, opened(c, d, [V.var(C.size(c))]))
def neutral_repr(head: V.head, spine: List<&2, V.spine>) -> K.repr:
  boolean(K.repr, Bool.and(V.head_equal(head, V.HGlobal{"Nat"}), empty(V.spine, spine)),
    _ => K.RUnion{K.Tid{"nat"}}, _ => any_repr)
def with_signature(-Out: Type, value: signature, next: List<&2, K.repr> -> K.repr -> Out) -> Out:
  match value:
    case Signature{ps, ret}: next(ps, ret)
def prepend(params: List<&2, K.repr>, sig: signature) -> signature:
  match sig:
    case Signature{rest, ret}: Signature{List.append(&2, K.repr, params, rest), ret}
def former_arrow(former: V.Former) -> Maybe<&2, arrow>:
  match former:
    case V.Former{shape, diagram, _}: point_arrow(diagram, R.point_view(shape))
def runtime_weak(c: C.ctx, +ty: V.t) -> R.result(Bool):
  maybe(L.t, R.result(Bool), V.as_univ(ty), _ =>
    boolean(R.result(Bool), Bool.or(empty_former(V.as_lan(ty)), empty_former(V.as_ran(ty))),
      _ => Done{False{}}, _ => proof_free(c, ty)), _ => Done{False{}})
def runtime_ty(+c: C.ctx, ty: V.t) -> R.result(Bool):
  R.bind(V.t, Bool, EV.whnf(C.globals(c), ty), +w => runtime_weak(c, w))
def point_runtime(c: C.ctx, q: Q.t, ty: V.t) -> R.result(Bool):
  boolean(R.result(Bool), Q.equal(q, Q.Zero{}), _ => Done{False{}}, _ => runtime_ty(c, ty))
def count(~A: Data, xs: List<&2, A>) -> F.Int63.t:
  match xs:
    case []: F.Int63.zero
    case _ <> rest: F.Int63.succ(count(A, rest))
# Kanon takes the first n legs and fails only with fewer. This port needs exactly n, which a checked diagram has, so a mismatch fails closed.
def leg_types(+c: C.ctx, +n: F.Int63.t, diagram: V.closure) -> R.result(List<&2, V.t>):
  R.bind(List<&2, V.vleg>, List<&2, V.t>, C.result_of(List<&2, V.vleg>, C.drive(List<&2, V.vleg>, R.coll_legs_of(C.ctx, C.ops, c, diagram), C.budget(c))), +legs =>
    boolean(R.result(List<&2, V.t>), F.Int63.equal(count(V.vleg, legs), n),
      _ => C.result_of(List<&2, V.t>, C.drive(List<&2, V.t>, R.map_list(C.ctx, V.vleg, V.t, C.ctx, +ctx => +leg => R.c_open_leg(C.ctx, C.ops, ctx, leg), c, legs), C.budget(c))),
      _ => Fail{E.Mismatch{"a collection diagram has the wrong number of legs"}}))
@unsafe
def tuple_texts(~loop: C.ctx -> V.t -> B.t -> R.result(K.repr), +c: C.ctx, tys: List<&2, V.t>, +b: B.t) -> R.result(List<&2, String>):
  match tys:
    case []: Done{[]}
    case +ty <> rest:
      R.bind(Bool, List<&2, String>, runtime_ty(c, ty), keep =>
        R.bind(List<&2, String>, List<&2, String>, boolean(R.result(List<&2, String>), keep,
          _ => R.map_result(K.repr, List<&2, String>, +r => [K.print_repr(r)], loop(c, ty, b)), _ => Done{[]}), +texts =>
          R.map_result(List<&2, String>, List<&2, String>, +tail => List.append(&2, String, texts, tail), tuple_texts(loop, c, rest, b))))
def tuple_repr(~loop: C.ctx -> V.t -> B.t -> R.result(K.repr), +c: C.ctx, +n: F.Int63.t, diagram: V.closure, +b: B.t) -> R.result(K.repr):
  boolean(R.result(K.repr), F.Int63.equal(n, F.Int63.zero), _ => Done{any_repr}, _ =>
    R.bind(List<&2, V.t>, K.repr, leg_types(c, n, diagram), +tys =>
      R.map_result(List<&2, String>, K.repr, +texts => K.RStruct{K.Tid{String.concat(["tuple<", K.join(",", texts), ">"])}}, tuple_texts(loop, c, tys, b))))
@unsafe
def sum_texts(~loop: C.ctx -> V.t -> B.t -> R.result(K.repr), +c: C.ctx, tys: List<&2, V.t>, +b: B.t) -> R.result(List<&2, String>):
  match tys:
    case []: Done{[]}
    case +ty <> rest:
      R.bind(Bool, List<&2, String>, runtime_ty(c, ty), keep =>
        R.bind(String, List<&2, String>, boolean(R.result(String), keep,
          _ => R.map_result(K.repr, String, K.print_repr, loop(c, ty, b)), _ => Done{"unit"}), +text =>
          R.map_result(List<&2, String>, List<&2, String>, +tail => text <> tail, sum_texts(loop, c, rest, b))))
def sum_repr(~loop: C.ctx -> V.t -> B.t -> R.result(K.repr), +c: C.ctx, +n: F.Int63.t, diagram: V.closure, +b: B.t) -> R.result(K.repr):
  boolean(R.result(K.repr), F.Int63.equal(n, F.Int63.zero), _ => Done{any_repr}, _ =>
    R.bind(List<&2, V.t>, K.repr, leg_types(c, n, diagram), +tys =>
      R.map_result(List<&2, String>, K.repr, +texts => K.RUnion{K.Tid{String.concat(["sum<", K.join("|", texts), ">"])}}, sum_texts(loop, c, tys, b))))
def refused_repr(+ty: V.t) -> R.result(K.repr):
  boolean(R.result(K.repr), Bool.or(has(V.Former, V.as_lan(ty)), has(V.Former, V.as_ran(ty))),
    _ => boolean(R.result(K.repr), Bool.or(empty_former(V.as_lan(ty)), empty_former(V.as_ran(ty))),
      _ => Done{any_repr}, _ => Fail{E.Not_yet{"structural layouts in type-directed erasure"}}),
    _ => Done{any_repr})
def arrow_of(value: V.t) -> Maybe<&2, arrow>:
  maybe(V.Former, Maybe<&2, arrow>, V.as_ran(value), _ => None{}, former_arrow)
def product_repr(~loop: C.ctx -> V.t -> B.t -> R.result(K.repr), c: C.ctx, ty: V.t, former: V.Former, b: B.t) -> R.result(K.repr):
  match former:
    case V.Former{shape, diagram, _}:
      maybe(F.Int63.t, R.result(K.repr), R.collection_view(shape), _ => refused_repr(ty), n => tuple_repr(loop, c, n, diagram, b))
@unsafe
def repr_text(~loop: C.ctx -> V.t -> B.t -> R.result(K.repr), c: C.ctx, ty: V.t, keep: Bool, b: B.t) -> R.result(List<&2, String>):
  boolean(R.result(List<&2, String>), keep,
    _ => R.map_result(K.repr, List<&2, String>, +r => [K.print_repr(r)], loop(c, ty, b)), _ => Done{[]})
def pair_repr(~loop: C.ctx -> V.t -> B.t -> R.result(K.repr), +c: C.ctx, point: arrow, +b: B.t) -> R.result(K.repr):
  match point:
    case Arrow{+q, x, +dom, d}:
      R.bind(Bool, K.repr, point_runtime(c, q, dom), keep1 =>
        R.bind(List<&2, String>, K.repr, repr_text(loop, c, dom, keep1, b), +first =>
          R.bind(F.Pair2<C.ctx, V.t>, K.repr, under(c, q, x, dom, d), value =>
            pair(C.ctx, V.t, R.result(K.repr), value, +c1 => +cod =>
              R.bind(Bool, K.repr, runtime_ty(c1, cod), keep2 =>
                R.map_result(List<&2, String>, K.repr, +second =>
                  K.RStruct{K.Tid{String.concat(["pair<", K.join(",", List.append(&2, String, first, second)), ">"])}}, repr_text(loop, c1, cod, keep2, b)))))))
def sum_former_repr(~loop: C.ctx -> V.t -> B.t -> R.result(K.repr), +c: C.ctx, ty: V.t, former: V.Former, +b: B.t) -> R.result(K.repr):
  match former:
    case V.Former{+shape, +diagram, _}:
      maybe(F.Pair2<String, List<&2, V.t>>, R.result(K.repr), R.as_mu(V.t, shape),
        _ => maybe(arrow, R.result(K.repr), point_arrow(diagram, R.point_view(shape)),
        _ => maybe(F.Int63.t, R.result(K.repr), R.collection_view(shape), _ => refused_repr(ty), n => sum_repr(loop, c, n, diagram, b)),
        point => pair_repr(loop, c, point, b)),
        value => pair(String, List<&2, V.t>, R.result(K.repr), value, name => _ => Done{K.RUnion{mu_tid(name)}}))
def structural_repr(~loop: C.ctx -> V.t -> B.t -> R.result(K.repr), +c: C.ctx, +ty: V.t, +b: B.t) -> R.result(K.repr):
  maybe(V.Former, R.result(K.repr), V.as_ran(ty),
    _ => maybe(V.Former, R.result(K.repr), V.as_lan(ty), _ => refused_repr(ty), former => sum_former_repr(loop, c, ty, former, b)),
    former => product_repr(loop, c, ty, former, b))
def nonfunction_repr(~loop: C.ctx -> V.t -> B.t -> R.result(K.repr), +c: C.ctx, +ty: V.t, +b: B.t) -> R.result(K.repr):
  maybe(F.Pair2<V.head, List<&2, V.spine>>, R.result(K.repr), V.as_neutral(ty), _ => structural_repr(loop, c, ty, b),
    +value => pair(V.head, List<&2, V.spine>, R.result(K.repr), value, +head => +spine => Done{neutral_repr(head, spine)}))
def arity_arrow(loop: C.ctx -> V.t -> F.Int63.t -> B.t -> R.result(F.Int63.t), +c: C.ctx, +n: F.Int63.t, a: arrow, b: B.t) -> R.result(F.Int63.t):
  match a:
    case Arrow{+q, x, +dom, d}:
      R.bind(Bool, F.Int63.t, point_runtime(c, q, dom), +keep =>
        R.bind(F.Pair2<C.ctx, V.t>, F.Int63.t, under(c, q, x, dom, d), +value =>
          pair(C.ctx, V.t, R.result(F.Int63.t), value, +c1 => +cod =>
            loop(c1, cod, F.choose(F.Int63.t, keep, F.Int63.succ(n), n), b))))
def arity_weak(loop: C.ctx -> V.t -> F.Int63.t -> B.t -> R.result(F.Int63.t), c: C.ctx, ty: V.t, +n: F.Int63.t, b: B.t) -> R.result(F.Int63.t):
  maybe(arrow, R.result(F.Int63.t), arrow_of(ty), _ => Done{n}, +a => arity_arrow(loop, c, n, a, b))
@unsafe
def arity(+c: C.ctx, ty: V.t, n: F.Int63.t, b: B.t) -> R.result(F.Int63.t):
  R.bind(B.t, F.Int63.t, B.spend("erasure function arity", b), +rest =>
    R.bind(V.t, F.Int63.t, EV.whnf(C.globals(c), ty), +w => arity_weak(+cc => +tt => +nn => +bb => arity(cc, tt, nn, bb), c, w, n, rest)))
def ordinary_repr(~loop: C.ctx -> V.t -> B.t -> R.result(K.repr), +c: C.ctx, +ty: V.t, +b: B.t) -> R.result(K.repr):
  maybe(arrow, R.result(K.repr), arrow_of(ty), _ => nonfunction_repr(loop, c, ty, b), _ =>
    R.map_result(F.Int63.t, K.repr, +n => K.RFunc{K.Tid{String.concat(["fn<", F.Int63.show(n), ">"])}}, arity(c, ty, F.Int63.zero, b)))
@unsafe
def repr_visit(+c: C.ctx, ty: V.t, b: B.t) -> R.result(K.repr):
  R.bind(B.t, K.repr, B.spend("erasure type representation", b), +rest =>
    R.bind(V.t, K.repr, EV.whnf(C.globals(c), ty), +w => ordinary_repr(+cc => +tt => +bb => repr_visit(cc, tt, bb), c, w, rest)))
def repr_of(+c: C.ctx, ty: V.t) -> R.result(K.repr): repr_visit(c, ty, C.budget(c))
def repr_tid(repr: K.repr) -> R.result(K.tid):
  match repr:
    case K.RI31{}: Fail{E.Mismatch{"a structured value stands at a type whose repr is i31"}}
    case K.RStruct{tid}: Done{tid}
    case K.RUnion{tid}: Done{tid}
    case K.RFunc{tid}: Done{tid}
    case K.RThunk{tid}: Done{tid}
def tid_of(c: C.ctx, ty: V.t) -> R.result(K.tid):
  R.bind(K.repr, K.tid, repr_of(c, ty), +repr => repr_tid(repr))
def parameter(+c: C.ctx, q: Q.t, +dom: V.t) -> R.result(List<&2, K.repr>):
  R.bind(Bool, List<&2, K.repr>, point_runtime(c, q, dom), +keep =>
    boolean(R.result(List<&2, K.repr>), keep, _ => R.map_result(K.repr, List<&2, K.repr>, +r => [r], repr_of(c, dom)), _ => Done{[]}))
def signature_arrow(loop: C.ctx -> V.t -> B.t -> R.result(signature), +c: C.ctx, a: arrow, b: B.t) -> R.result(signature):
  match a:
    case Arrow{+q, x, +dom, d}:
      R.bind(List<&2, K.repr>, signature, parameter(c, q, dom), +here =>
        R.bind(F.Pair2<C.ctx, V.t>, signature, under(c, q, x, dom, d), +value =>
          pair(C.ctx, V.t, R.result(signature), value, +c1 => +cod =>
            R.map_result(signature, signature, +s => prepend(here, s), loop(c1, cod, b)))))
def signature_weak(loop: C.ctx -> V.t -> B.t -> R.result(signature), +c: C.ctx, +ty: V.t, b: B.t) -> R.result(signature):
  maybe(arrow, R.result(signature), arrow_of(ty),
    _ => R.map_result(K.repr, signature, +r => Signature{[], r}, repr_of(c, ty)), +a => signature_arrow(loop, c, a, b))
@unsafe
def signature_of(+c: C.ctx, ty: V.t, b: B.t) -> R.result(signature):
  R.bind(B.t, signature, B.spend("erasure function signature", b), +rest =>
    R.bind(V.t, signature, EV.whnf(C.globals(c), ty), +w => signature_weak(+cc => +tt => +bb => signature_of(cc, tt, bb), c, w, rest)))
