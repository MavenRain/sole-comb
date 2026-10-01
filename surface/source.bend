# Object-language admission before parsing. Comments and byte literals are
# already tokens, so reserved words inside them are not mistaken for syntax.
import Base
import token.bend as T
import lexer.bend as L

def tokens(ts: List<&2, T.t>) -> L.result(List<&2, T.t>):
  match ts:
    case []: Done{[]}
    case T.Token{+kind, +loc} <> rest:
      match kind:
        case T.KCase{}: L.error(List<&2, T.t>, loc, "E-R4-MATCH: 'case' is reserved; use elim")
        case T.KMatch{}: L.error(List<&2, T.t>, loc, "E-R4-MATCH: 'match' is reserved; use elim")
        case T.KRec{}: L.error(List<&2, T.t>, loc, "E-R4-MATCH: 'rec' is reserved; use elim")
        case T.Pipe{}: L.error(List<&2, T.t>, loc, "E-R4-MATCH: branch-binder syntax is reserved; use elim arms")
        case T.Dot1{}: L.error(List<&2, T.t>, loc, "E-R4-MATCH: '.1' is reserved")
        case T.Dot2{}: L.error(List<&2, T.t>, loc, "E-R4-MATCH: '.2' is reserved")
        case T.KMu{}: L.error(List<&2, T.t>, loc, "E-SOURCE-NOT-YET: recursive families need the M0 recursor semantics")
        case T.KNu{}: L.error(List<&2, T.t>, loc, "E-SOURCE-NOT-YET: nu declarations are not supported")
        case T.KMutual{}: L.error(List<&2, T.t>, loc, "E-SOURCE-NOT-YET: mutual families need the M0 recursor semantics")
        case T.KWith{}: L.bind(List<&2, T.t>, List<&2, T.t>, tokens(rest), tail => Done{T.Token{T.Ident{"with"}, loc} <> tail})
        case T.KAbsurd{}: L.bind(List<&2, T.t>, List<&2, T.t>, tokens(rest), tail => Done{T.Token{T.Ident{"absurd"}, loc} <> tail})
        case T.KEnd{}: L.bind(List<&2, T.t>, List<&2, T.t>, tokens(rest), tail => Done{T.Token{T.Ident{"end"}, loc} <> tail})
        case other: L.bind(List<&2, T.t>, List<&2, T.t>, tokens(rest), tail => Done{T.Token{other, loc} <> tail})
