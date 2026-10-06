import Base
import ./foundation.bend as F
import ./kernel_positivity.bend as P
import ./kernel_term.bend as T

# The group holds the families whose constructor fields reach the owner. Families are
# declared in order, so an owner field can mention only the owner and its mutual siblings.
def reaches(+names: List<&2, String>, ctors: List<&2, P.ctor>) -> Bool:
  match ctors:
    case []: False{}
    case P.Ctor{_, args, _, _, _} <> rest: Bool.or(P.self_rec(names, args), reaches(names, rest))

def admit(yes: Bool, n: String, +tail: List<&2, String>) -> List<&2, String>:
  F.choose(List<&2, String>, yes, n <> tail, tail)

def joining(+names: List<&2, String>, fams: List<&2, F.Pair2<String, P.family>>) -> List<&2, String>:
  match fams:
    case []: []
    case F.Pair2{+n, P.Family{_, _, _, _, _, ctors, _}} <> rest:
      admit(Bool.and(Bool.not(T.member(n, names)), reaches(names, ctors)), n, joining(names, rest))

def extend(+names: List<&2, String>, added: List<&2, String>, next: List<&2, String> -> List<&2, String>) -> List<&2, String>:
  match added:
    case []: names
    case n <> more: next(List.append(&2, String, n <> more, names))

# A round that adds no family is the fixpoint. Every other round adds one or more families,
# so one round of fuel for each declared family is sufficient.
def reach(fuel: List<&2, F.Pair2<String, P.family>>, +fams: List<&2, F.Pair2<String, P.family>>, +names: List<&2, String>) -> List<&2, String>:
  match fuel:
    case []: names
    case _ <> rest: extend(names, joining(names, fams), grown => reach(rest, fams, grown))

def members(+fams: List<&2, F.Pair2<String, P.family>>, owner: String) -> List<&2, String>:
  reach(fams, fams, [owner])
