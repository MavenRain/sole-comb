# A suspended surface walk, following the kernel's request/resume discipline.
import Base
import ../../lib/kernel_error.bend as E
import ../../lib/kernel_check.bend as K
import ../../lib/kernel_rules.bend as R
import ../../lib/kernel_term.bend as T
import ../../lib/kernel_value.bend as V
import syntax.bend as S

type program<-A: Data> is Type:
  Pure{value: A}
  Abort{error: E.t}
  Request{context: K.ctx, expected: Maybe<&2, V.t>, syntax: S.t, resume: R.result(T.t) -> program<A>}
def result(-A: Data) -> Type: program<A>
def lift(-A: Data, value: R.result(A)) -> program<A>:
  match value:
    case Fail{error}: Abort{error}
    case Done{x}: Pure{x}
def request(c: K.ctx, expected: Maybe<&2, V.t>, s: S.t) -> program<T.t>:
  Request{c, expected, s, value => lift(T.t, value)}

# A continuation's program is available only after its request is answered.
@unsafe
def bind(-A: Data, -D: Data, value: program<A>, next: A -> program<D>) -> program<D>:
  match value:
    case Pure{x}: next(x)
    case Abort{error}: Abort{error}
    case Request{c, expected, s, resume}: Request{c, expected, s, answer => bind(A, D, resume(answer), next)}
def map_result(-A: Data, -D: Data, f: A -> D, value: program<A>) -> program<D>:
  bind(A, D, value, x => Pure{f(x)})
