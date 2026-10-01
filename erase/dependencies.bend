# Layout dependencies from Kanon 69f3be51 lib/erase.ml:tids_ktm.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_rules.bend as R
import eterm.bend as K
import runtime.bend as RT

def own(tm: K.ktm, acc: List<&2, K.tid>) -> List<&2, K.tid>:
  match tm:
    case K.KVar{_}: acc
    case K.KLit{_}: acc
    case K.KGlobal{_}: acc
    case K.KErased{}: acc
    case K.KLet{_, _, _}: acc
    case K.KClos{_, _, _}: acc
    case K.KApp{_, _}: acc
    case K.KTail{_, _}: acc
    case K.KStruct{tid, _}: tid <> acc
    case K.KProj{tid, _, _}: tid <> acc
    case K.KTag{tid, _, _}: tid <> acc
    case K.KCase{tid, _, _}: tid <> acc
    case K.KDelay{_, _}: acc
    case K.KForce{_}: acc
@unsafe
def loop(pending: List<&2, F.Pair2<F.Int63.t, K.ktm>>, acc: List<&2, K.tid>) -> List<&2, K.tid>:
  match pending:
    case []: R.rev_append(K.tid, acc, [])
    case F.Pair2{depth, +tm} <> rest:
      loop(List.append(&2, F.Pair2<F.Int63.t, K.ktm>, RT.children(depth, tm), rest), own(tm, acc))
def term(tm: K.ktm) -> List<&2, K.tid>: loop([F.Pair2{F.Int63.zero, tm}], [])
