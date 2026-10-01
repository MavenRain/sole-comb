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
