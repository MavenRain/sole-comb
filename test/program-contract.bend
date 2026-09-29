# Small direct boundary checks complement the complete surface corpus.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_budget.bend as B
import ../lib/kernel_error.bend as E
import ../lib/kernel_global.bend as G
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_rules.bend as R
import ../lib/kernel_shape.bend as S
import ../lib/kernel_term.bend as T
import ./pinfront/order.bend as O
import ./pinfront/totality.bend as Totality

def ix(n: Nat) -> F.Int63.t: O.index(n)
def nty() -> T.t: T.Global{"N"}
def point() -> S.t<T.t>: S.SPi{Q.Many{}, "x", nty}
def lam(body: T.t) -> T.t: T.Sec{point, [T.Leg{[F.Pair2{Q.Many{}, "x"}], body}]}
def app(head: T.t, arg: T.t) -> T.t: T.Out{point, T.APt{Q.Many{}, arg}, head}
def self(arg: T.t) -> T.t: app(T.Global{"f"}, arg)
def motive() -> Maybe<&2, T.motive>: Some{T.Motive{Some{"N"}, [], "x", nty}}
def elim(mo: Maybe<&2, T.motive>, key: T.addr, body: T.t) -> T.t:
  T.Elim{T.Elimination{S.SMu{"N", []}, T.Var{ix(0n)}, Q.Many{}, mo, [F.Pair2{key, T.Leg{[F.Pair2{Q.Many{}, "m"}], body}}]}}
def recursive() -> T.t: lam(elim(motive, T.ACtor{"succ"}, self(T.Var{ix(0n)})))
def cert(body: T.t) -> R.result(Maybe<&2, O.t>): O.certify(O.Structural{}, [F.Pair2{"f", body}])
def success(-A: Data, value: R.result(A)) -> Bool:
  match value:
    case Fail{_}: False{}
    case Done{_}: True{}
def error_text(-A: Data, expected: String, value: R.result(A)) -> Bool:
  match value:
    case Done{_}: False{}
    case Fail{e}: String.eq(expected, E.to_string(e))
def no_certificate(value: R.result(Maybe<&2, O.t>)) -> Bool:
  match value:
    case Done{None{}}: True{}
    case _: False{}
def arg_is(n: Nat, value: R.result(Maybe<&2, O.t>)) -> Bool:
  match value:
    case Done{Some{O.Certificate{_, k, _}}}: F.Int63.equal(k, ix(n))
    case _: False{}
def chain_is(value: R.result(Maybe<&2, O.t>)) -> Bool:
  match value:
    case Done{Some{O.Certificate{_, _, [O.Row{_, [O.Call{callee, [O.Step{scrut, Some{ctor}}]}]}]}}}:
      F.choose(Bool, String.eq(callee, "f"), F.choose(Bool, String.eq(ctor, "succ"), F.Int63.equal(scrut, ix(0n)), False{}), False{})
    case _: False{}
def translated(+body: T.t) -> R.result(T.t):
  R.bind(Maybe<&2, O.t>, T.t, cert(body), value => O.maybe(O.t, R.result(T.t), value, _ => Fail{E.Termination{"no certificate"}}, c => O.translate(c, body)))
def check(name: String, good: Bool) -> String: F.choose(String, good, "", String.append(name, "\n"))
def errors() -> String:
  String.concat([
    check("empty-budget", success(Unit, Totality.spend(0n, []))),
    check("zero-budget", error_text(Unit, "budget: the totality budget is exhausted", Totality.spend(0n, [T.Auto{}]))),
    check("one-node", success(Unit, Totality.spend(1n, [T.Auto{}]))),
    check("nested-budget", error_text(Unit, "budget: the totality budget is exhausted", Totality.spend(2n, [T.Ann{T.Auto{}, T.Auto{}}]))),
    check("exact-budget", success(Unit, Totality.spend(3n, [T.Ann{T.Auto{}, T.Auto{}}]))),
    check("empty-group", no_certificate(Totality.guard_group(B.create(0n), G.empty, []))),
    check("no-group-call", no_certificate(cert(nty))),
    check("budget-before-guard", error_text(Maybe<&2, O.t>, "budget: the totality budget is exhausted", Totality.guard_group(B.create(0n), G.empty, [F.Pair2{"f", T.Global{"f"}}]))),
    check("bare-self", error_text(Maybe<&2, O.t>, "termination: recursive definition f failed the structural termination guard", cert(T.Global{"f"}))),
    check("first-position", arg_is(0n, cert(recursive))),
    check("second-position", arg_is(1n, cert(lam(lam(elim(motive, T.ACtor{"succ"}, app(self(T.Global{"z"}), T.Var{ix(0n)}))))))),
    check("constructor-chain", chain_is(cert(recursive))),
    check("helper-without-formals", arg_is(0n, O.certify(O.Structural{}, [F.Pair2{"helper", nty}, F.Pair2{"f", recursive}]))),
    check("missing-motive", error_text(T.t, "missing branch: the elimination of a structurally recursive definition carries its motive", translated(lam(elim(None{}, T.ACtor{"succ"}, self(T.Var{ix(0n)})))))),
    check("constructor-key", error_text(T.t, "wrong leg: every branch of the elimination of a structurally recursive definition keys a constructor", translated(lam(elim(motive, T.ALeg{ix(0n)}, self(T.Var{ix(0n)})))))),
    check("translated-guard", success(T.t, translated(recursive)))])
def finish(+failures: String) -> String:
  F.choose(String, String.eq(failures, ""), "PASS program contracts: 16", String.append("FAIL program contracts:\n", failures))
def report() -> String: finish(errors)
