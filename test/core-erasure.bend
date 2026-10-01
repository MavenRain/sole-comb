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
import ../lib/kernel_term.bend as T
import ../lib/kernel_value.bend as V
import ../erase/core.bend as Erase
import ../erase/type.bend as Ty
import ../erase/eterm.bend as K

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
def main() -> String:
  String.concat([
    check("proof", "false", proof),
    check("type", "false", boolean(Ty.runtime_ty(context, V.VUniv{L.one}))),
    check("nat", "true", boolean(Ty.runtime_ty(context, nat))),
    check("zero", "false", boolean(Ty.point_runtime(context, Q.Zero{}, nat))),
    check("one", "true", boolean(Ty.point_runtime(context, Q.One{}, nat))),
    check("budget", "budget: type-directed erasure", exhausted),
    check("representation-budget", "budget: erasure type representation", repr_exhausted),
    "PASS"])
