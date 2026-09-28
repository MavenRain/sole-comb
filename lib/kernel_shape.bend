# Closed shape representation from Kanon 69f3be51 shape.ml.
import Base
import ./foundation.bend as F
import ./kernel_quantity.bend as Q

type t<-A: Data> is Data:
  SPi{quantity: Q.t, binder: String, domain: A}
  SColl{size: F.Int63.t}
  SPar{left: A, right: A}
  SMu{family: String, indices: List<&2, A>}
  SNu{family: String, indices: List<&2, A>}

def declared() -> List<&2, String>:
  ["SPi", "SColl", "SPar", "SMu", "SNu"]

def payload(-A: Data, shape: t<A>) -> List<&2, A>:
  match shape:
    case SPi{_, _, domain}: [domain]
    case SColl{_}: []
    case SPar{left, right}: [left, right]
    case SMu{_, indices}: indices
    case SNu{_, indices}: indices

def point_dom(-A: Data, shape: t<A>) -> Maybe<&2, A>:
  match shape:
    case SPi{_, _, domain}: Some{domain}
    case SColl{_}: None{}
    case SPar{_, _}: None{}
    case SMu{_, _}: None{}
    case SNu{_, _}: None{}

def family(-A: Data, shape: t<A>) -> Maybe<&2, String>:
  match shape:
    case SPi{_, _, _}: None{}
    case SColl{_}: None{}
    case SPar{_, _}: None{}
    case SMu{name, _}: Some{name}
    case SNu{name, _}: Some{name}

def name(-A: Data, shape: t<A>) -> String:
  match shape:
    case SPi{_, _, _}: "SPi"
    case SColl{_}: "SColl"
    case SPar{_, _}: "SPar"
    case SMu{_, _}: "SMu"
    case SNu{_, _}: "SNu"
