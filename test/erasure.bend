import Base
import ../lib/foundation.bend as F
import ../erase/eterm.bend as K
import ../erase/runtime.bend as R
import foundation.bend as FT

def number(text: String) -> F.Bignum.t: FT.number(text)
def force_chain(count: Nat, body: K.ktm) -> K.ktm:
  match count:
    case Zero{}: body
    case Succ{n}: K.KForce{force_chain(n, body)}
def let_chain(count: Nat, body: K.ktm) -> K.ktm:
  match count:
    case Zero{}: body
    case Succ{n}: K.KLet{"x", K.KVar{F.Int63.zero}, let_chain(n, body)}
def reindex(~index: F.Int63.t -> F.Int63.t, depth: F.Int63.t, t: K.ktm) -> R.result<K.ktm>:
  R.reindex_runtime(Unit, _ => i => index(i), Unit{}, depth, t)
def mapping(value: R.result<K.ktm>) -> String:
  match value:
    case R.InvalidStack{}: "invalid runtime stack"
    case R.Done{t}: K.print_ktm(t)
def integers(xs: List<&2, F.Int63.t>) -> List<&2, String>:
  match xs:
    case []: []
    case x <> xs: F.Int63.show(x) <> integers(xs)
def vars(depth: F.Int63.t, t: K.ktm) -> String:
  K.cat(["[", K.join("; ", integers(R.runtime_vars(depth, t))), "]"])
def pruned(value: R.result<R.pruned>) -> String:
  match value:
    case R.InvalidStack{}: "invalid runtime stack"
    case R.Done{R.Pruned{ps, args, body}}: K.cat(["[", K.join("; ", K.repr_texts(ps)), "]|", K.ktm_list(args), "|", K.print_ktm(body)])
def boolean(value: Bool) -> String: F.choose(String, value, "true", "false")
def main() -> String: K.print_ktm(K.KErased{})
