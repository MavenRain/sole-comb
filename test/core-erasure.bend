import Base
import ../lib/foundation.bend as F
import ../lib/kernel_budget.bend as B
import ../lib/kernel_check.bend as C
import ../lib/kernel_error.bend as E
import ../lib/kernel_global.bend as G
import ../lib/kernel_level.bend as L
import ../lib/kernel_literal.bend as Lit
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_rules.bend as R
import ../lib/kernel_shape.bend as S
import ../lib/kernel_term.bend as T
import ../lib/kernel_value.bend as V
import ../erase/core.bend as Erase
import ../erase/type.bend as Ty
import ../erase/eterm.bend as K
import ../erase/sum.bend as Sum
import ../erase/pair.bend as Pair

def check(+name: String, +want: String, +got: String) -> String:
  F.choose(String, String.eq(want, got), "", String.concat([name, " expected=", want, " actual=", got, "\n"]))
def boolean(value: R.result(Bool)) -> String:
  match value:
    case Fail{error}: E.to_string(error)
    case Done{value}: F.choose(String, value, "true", "false")
def context() -> C.ctx: C.make(G.initial, B.create(16384n))
def nat() -> V.t: V.VNeutral{V.HGlobal{"Nat"}, []}
def proof_globals() -> G.t: G.add("P", G.Axiom{G.Postulate{T.Univ{L.zero}}}, G.initial)
def proof() -> String:
  boolean(Ty.runtime_ty(C.make(proof_globals, B.create(16384n)), V.VNeutral{V.HGlobal{"P"}, []}))
def budget(value: R.result(Erase.answer)) -> String:
  match value:
    case Fail{error}: E.to_string(error)
    case Done{_}: "unexpected erasure success"
def exhausted() -> String:
  budget(Erase.term(Erase.Context{context, [], "probe"}, Erase.State{0n, [], B.create(0n)}, False{}, Some{nat}, T.Lit{Lit.LInt{F.Bignum.one}}))
def repr_budget(value: R.result(K.repr)) -> String:
  match value:
    case Fail{error}: E.to_string(error)
    case Done{_}: "unexpected representation success"
def repr_exhausted() -> String: repr_budget(Ty.repr_visit(context, nat, B.create(0n)))
def result_error(-A: Data, value: R.result(A)) -> String:
  match value:
    case Fail{error}: E.to_string(error)
    case Done{_}: "unexpected success"
def two() -> F.Int63.t: F.Int63.succ(F.Int63.one)
def literal(n: Nat) -> T.t: T.Lit{Lit.LInt{F.Bignum.from_nat(n)}}
def nat_leg() -> T.leg: T.Leg{[], T.Global{"Nat"}}
def sum_term() -> T.t: T.Lan{S.SColl{two}, T.Sec{S.SColl{two}, [nat_leg, nat_leg]}}
def sum_value() -> V.t: V.VLan{S.SColl{two}, V.close([], T.Sec{S.SColl{two}, [nat_leg, nat_leg]}), Some{L.zero}}
def branch(n: Nat) -> T.leg: T.Leg{[F.Pair2{Q.Many{}, "x"}], literal(n)}
def reordered_branches() -> List<&2, F.Pair2<T.addr, T.leg>>:
  [F.Pair2{T.ALeg{F.Int63.one}, branch(20n)}, F.Pair2{T.ALeg{F.Int63.zero}, branch(10n)}]
def term_text(value: R.result(Erase.answer)) -> String:
  match value:
    case Fail{error}: E.to_string(error)
    case Done{F.Pair2{body, _}}: K.print_ktm(body)
def reordered() -> String:
  term_text(Erase.term(Erase.Context{C.make(G.add("tag", G.Axiom{G.Postulate{sum_term}}, G.initial), B.create(16384n)), [], "probe"},
    Erase.State{0n, [], B.create(16384n)}, True{}, Some{nat},
    T.Elim{T.Elimination{S.SColl{two}, T.Global{"tag"}, Q.Many{}, None{}, reordered_branches}}))
def malformed_branch(binders: List<&2, F.Pair2<Q.t, String>>) -> String:
  result_error(Sum.branch, Sum.branch_of(context, nat, None{}, [nat, nat], F.Int63.zero,
    [F.Pair2{T.ALeg{F.Int63.zero}, T.Leg{binders, literal(10n)}}]))
def malformed_diagram() -> String:
  result_error(List<&2, V.t>, Sum.legs_of(context, Sum.Collection{two, V.close([], T.Sec{S.SColl{F.Int63.one}, [nat_leg]})}))
def repr_text(value: R.result(K.repr)) -> String:
  match value:
    case Fail{error}: E.to_string(error)
    case Done{repr}: K.print_repr(repr)
def sum_repr() -> String: repr_text(Ty.repr_of(context, sum_value))
def pair_term() -> T.t: T.Lan{S.SPi{Q.Many{}, "x", T.Global{"Nat"}}, T.Global{"Nat"}}
def pair_value() -> V.t: V.VLan{S.SPi{Q.Many{}, "x", nat}, V.close([], T.Global{"Nat"}), Some{L.one}}
def pair_context() -> C.ctx: C.make(G.add("pair", G.Axiom{G.Postulate{pair_term}}, G.initial), B.create(16384n))
def malformed_pair(binders: List<&2, F.Pair2<Q.t, String>>) -> String:
  result_error(Pair.branch, Pair.elimination(pair_context, nat, S.SPi{Q.Many{}, "x", T.Global{"Nat"}}, T.Global{"pair"}, None{},
    [F.Pair2{T.ALeg{F.Int63.zero}, T.Leg{binders, literal(10n)}}]))
def synthetic_variable() -> String:
  term_text(Erase.term(Erase.Context{context, [Erase.Keep{}, Erase.Keep{}, Erase.Extra{}, Erase.Keep{}], "probe"},
    Erase.State{0n, [], B.create(16384n)}, False{}, Some{nat}, T.Var{two}))
def synthetic_frame() -> String:
  term_text(Erase.term(Erase.Context{context, Erase.framed([Erase.Keep{}, Erase.Extra{}, Erase.Keep{}], [F.Int63.one], F.Int63.zero), "probe"},
    Erase.State{0n, [], B.create(16384n)}, False{}, Some{nat}, T.Var{F.Int63.one}))
def pair_closure() -> String:
  term_text(Erase.term(Erase.Context{C.bind("outer", Q.Many{}, nat, pair_context), [Erase.Keep{}], "probe"},
    Erase.State{0n, [], B.create(16384n)}, True{}, Some{V.VRan{S.SPi{Q.Many{}, "n", nat}, V.close([], T.Global{"Nat"}), Some{L.one}}},
    T.Elim{T.Elimination{S.SPi{Q.Many{}, "x", T.Global{"Nat"}}, T.Global{"pair"}, Q.One{}, None{},
      [F.Pair2{T.ALeg{F.Int63.zero}, T.Leg{[F.Pair2{Q.Many{}, "x"}, F.Pair2{Q.Many{}, "y"}],
        T.Sec{S.SPi{Q.Many{}, "n", T.Global{"Nat"}}, [T.Leg{[F.Pair2{Q.Many{}, "n"}], T.Var{F.Int63.succ(two)}}]}}}]}}))
def main() -> String:
  String.concat([
    check("proof", "false", proof),
    check("type", "false", boolean(Ty.runtime_ty(context, V.VUniv{L.one}))),
    check("nat", "true", boolean(Ty.runtime_ty(context, nat))),
    check("zero", "false", boolean(Ty.point_runtime(context, Q.Zero{}, nat))),
    check("one", "true", boolean(Ty.point_runtime(context, Q.One{}, nat))),
    check("budget", "budget: type-directed erasure", exhausted),
    check("representation-budget", "budget: erasure type representation", repr_exhausted),
    check("sum-layout", "union sum<union nat|union nat>", sum_repr),
    check("pair-layout", "struct pair<union nat,union nat>", repr_text(Ty.repr_of(context, pair_value))),
    check("pair-address", "mismatch: a pair takes a point address", result_error(List<&2, Pair.field>, Pair.introduction(context, pair_value, T.ALeg{F.Int63.zero}, [literal(1n)]))),
    check("pair-no-fibre", "mismatch: a pair takes one fibre", result_error(List<&2, Pair.field>, Pair.introduction(context, pair_value, T.APt{Q.Many{}, literal(1n)}, []))),
    check("pair-two-fibres", "mismatch: a pair takes one fibre", result_error(List<&2, Pair.field>, Pair.introduction(context, pair_value, T.APt{Q.Many{}, literal(1n)}, [literal(1n), literal(2n)]))),
    check("pair-no-binders", "mismatch: a pair branch takes two binders", malformed_pair([])),
    check("pair-one-binder", "mismatch: a pair branch takes two binders", malformed_pair([F.Pair2{Q.Many{}, "x"}])),
    check("synthetic-index", "KVar 3", synthetic_variable),
    check("synthetic-frame", "KVar 0", synthetic_frame),
    check("pair-closure", "KLet scrut (KGlobal pair) (KLet x (KProj pair<union nat,union nat> 0 (KVar 0)) (KLet y (KProj pair<union nat,union nat> 1 (KVar 1)) (KClos probe$0 1 [KVar 3])))", pair_closure),
    check("reordered-branches", "KCase sum<union nat|union nat> (KGlobal tag) [{0 1 (KLit 10)}; {1 1 (KLit 20)}]", reordered),
    check("injection-address", "mismatch: an injection takes a leg address", result_error(Sum.tag, Sum.injection(context, sum_value, T.ACtor{"bad"}, [literal(1n)]))),
    check("injection-negative", "mismatch: the leg address is outside the collection", result_error(Sum.tag, Sum.injection(context, sum_value, T.ALeg{F.Int63.Int{True{}, [1]}}, [literal(1n)]))),
    check("injection-outside", "mismatch: the leg address is outside the collection", result_error(Sum.tag, Sum.injection(context, sum_value, T.ALeg{two}, [literal(1n)]))),
    check("injection-no-payload", "mismatch: an injection takes one payload", result_error(Sum.tag, Sum.injection(context, sum_value, T.ALeg{F.Int63.zero}, []))),
    check("injection-two-payloads", "mismatch: an injection takes one payload", result_error(Sum.tag, Sum.injection(context, sum_value, T.ALeg{F.Int63.zero}, [literal(1n), literal(2n)]))),
    check("injection-former", "mismatch: a sum stands at a type that is not a left former", result_error(Sum.tag, Sum.injection(context, nat, T.ALeg{F.Int63.zero}, [literal(1n)]))),
    check("case-missing", "mismatch: a case is missing a branch", result_error(Sum.branch, Sum.branch_of(context, nat, None{}, [nat, nat], F.Int63.zero, []))),
    check("case-outside", "mismatch: a leg number is outside the collection", result_error(Sum.branch, Sum.branch_of(context, nat, None{}, [nat, nat], two, reordered_branches))),
    check("case-no-binder", "mismatch: a case branch takes one binder", malformed_branch([])),
    check("case-two-binders", "mismatch: a case branch takes one binder", malformed_branch([F.Pair2{Q.Many{}, "x"}, F.Pair2{Q.Many{}, "y"}])),
    check("sum-diagram-count", "mismatch: a collection diagram has the wrong number of legs", malformed_diagram),
    "PASS"])
