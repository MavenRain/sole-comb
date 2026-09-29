# Finite node polling followed by the pinned structural certificate.
import Base
import ../../lib/foundation.bend as F
import ../../lib/kernel_budget.bend as B
import ../../lib/kernel_error.bend as E
import ../../lib/kernel_global.bend as G
import ../../lib/kernel_term.bend as T
import ../../lib/kernel_rules.bend as R
import ./order.bend as O

def budget_msg() -> String: "the totality budget is exhausted"
def pending(scan: T.Scan) -> List<&2, T.t>:
  match scan:
    case T.Scan{_, next}: next
def empty(terms: List<&2, T.t>) -> R.result(Unit):
  match terms:
    case []: Done{Unit{}}
    case _ <> _: Fail{E.Budget_exhausted{budget_msg}}
def next_terms(terms: List<&2, T.t>) -> Maybe<&2, List<&2, T.t>>:
  match terms:
    case []: None{}
    case tm <> rest: Some{pending(T.scan_term(False{}, [], tm, rest))}
def spend(fuel: Nat, terms: List<&2, T.t>) -> R.result(Unit):
  match fuel:
    case 0n: empty(terms)
    case 1n+remaining: O.maybe(List<&2, T.t>, R.result(Unit), next_terms(terms), _ => Done{Unit{}}, next => spend(remaining, next))
def bodies(ms: List<&2, F.Pair2<String, T.t>>) -> List<&2, T.t>:
  match ms:
    case []: []
    case F.Pair2{_, tm} <> rest: tm <> bodies(rest)
def guard_group(budget: B.t, globals: G.t, +members: List<&2, F.Pair2<String, T.t>>) -> R.result(Maybe<&2, O.t>):
  match budget:
    case B.Budget{fuel}: R.bind(Unit, Maybe<&2, O.t>, spend(fuel, bodies(members)), _ => O.certify(O.Structural{}, members))
def guarded_arg(c: O.t) -> F.Int63.t:
  match c:
    case O.Certificate{_, arg, _}: arg
def guard_result(value: R.result(Maybe<&2, O.t>)) -> R.result(Maybe<&2, F.Int63.t>):
  match value:
    case Fail{e}: Fail{e}
    case Done{None{}}: Done{None{}}
    case Done{Some{c}}: Done{Some{guarded_arg(c)}}
def guard(budget: B.t, globals: G.t, name: String, ty: T.t, body: T.t) -> R.result(Maybe<&2, F.Int63.t>):
  guard_result(guard_group(budget, globals, [F.Pair2{name, body}]))
