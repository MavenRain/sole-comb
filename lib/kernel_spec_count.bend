# Every count is derived from its printed inventory at the Kanon pin.
import Base
import ./foundation.bend as F
import ./kernel_term.bend as T
import ./kernel_shape.bend as S
import ./kernel_rules.bend as R

def pick(xs: List<&2, String>) -> String:
  match xs:
    case []: "?"
    case x <> _: x
def tail(xs: List<&2, String>) -> List<&2, String>:
  match xs:
    case []: []
    case _ <> rest: rest
def lan() -> String: pick(T.formers)
def ran() -> String: pick(tail(T.formers))
def admitted_of(packs: List<&2, F.Pair2<String, R.pack>>) -> List<&2, String>:
  match packs:
    case []: []
    case F.Pair2{name, _} <> rest: name <> admitted_of(rest)
def admitted() -> List<&2, String>: admitted_of(R.packs)
def include_row(keep: Bool, name: String, tail: List<&2, String>) -> List<&2, String>:
  match keep:
    case False{}: tail
    case True{}: name <> tail
def eta_of(+keep: Bool, packs: List<&2, F.Pair2<String, R.pack>>) -> List<&2, String>:
  match packs:
    case []: []
    case F.Pair2{+name, +pack} <> rest:
      include_row(Bool.not(Bool.xor(R.has_eta(pack, True{}), keep)), String.concat([ran, "-", name]),
        include_row(Bool.not(Bool.xor(R.has_eta(pack, False{}), keep)), String.concat([lan, "-", name]), eta_of(keep, rest)))
def row(label: String, +items: List<&2, String>) -> String:
  String.concat([label, " ", Nat.show(List.length(&2, String, items)), ": ", String.join(items, " "), "\n"])
def print() -> String:
  String.concat([row("formers", T.formers), row("schema constructors", T.schema), row("shapes declared", S.declared), row("shapes admitted", admitted), row("named rules declared", R.named_declared), row("named rules present", R.named_present), row("eta rows", eta_of(True{}, R.packs)), row("no eta", eta_of(False{}, R.packs))])
