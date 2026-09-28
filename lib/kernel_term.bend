# Syntax uses signed host indices, as at Kanon 69f3be51 term.ml.
import Base
import ./foundation.bend as F
import ./kernel_level.bend as L
import ./kernel_literal.bend as Lit
import ./kernel_quantity.bend as Q
import ./kernel_shape.bend as S

type AddrData<-A: Data> is Data:
  APt{quantity: Q.t, arg: A}
  ALeg{index: F.Int63.t}
  ACtor{name: String}

type LegData<-A: Data> is Data:
  Leg{l_binders: List<&2, F.Pair2<Q.t, String>>, l_body: A}

# Indices are outermost first; m_self is index zero in m_body.
type MotiveData<-A: Data> is Data:
  Motive{m_ind: Maybe<&2, String>, m_idx: List<&2, String>, m_self: String, m_body: A}

type ElimData<-A: Data> is Data:
  Elimination{e_shape: S.t<A>, e_scrut: A, e_scrut_q: Q.t, e_motive: Maybe<&2, MotiveData<A>>, e_branches: List<&2, F.Pair2<AddrData<A>, LegData<A>>>}

type t is Data:
  Var{index: F.Int63.t}
  Univ{level: L.t}
  Lan{shape: S.t<t>, diagram: t}
  Ran{shape: S.t<t>, diagram: t}
  In{shape: S.t<t>, address: AddrData<t>, args: List<&2, t>}
  Elim{elim: ElimData<t>}
  Sec{shape: S.t<t>, legs: List<&2, LegData<t>>}
  Out{shape: S.t<t>, address: AddrData<t>, head: t}
  Let{name: String, ty: t, value: t, body: t}
  Ann{term: t, ty: t}
  Global{name: String}
  Lit{literal: Lit.t}
  Auto{}

def addr() -> Data: AddrData<t>
def leg() -> Data: LegData<t>
def motive() -> Data: MotiveData<t>
def elim() -> Data: ElimData<t>
def formers() -> List<&2, String>: ["Lan", "Ran"]
def schema() -> List<&2, String>: ["In", "Elim", "Sec", "Out"]

def as_apt(address: addr) -> Maybe<&2, F.Pair2<Q.t, t>>:
  match address:
    case APt{q, arg}: Some{F.Pair2{q, arg}}
    case ALeg{_}: None{}
    case ACtor{_}: None{}

def as_aleg(address: addr) -> Maybe<&2, F.Int63.t>:
  match address:
    case APt{_, _}: None{}
    case ALeg{index}: Some{index}
    case ACtor{_}: None{}

def as_actor(address: addr) -> Maybe<&2, String>:
  match address:
    case APt{_, _}: None{}
    case ALeg{_}: None{}
    case ACtor{name}: Some{name}

def member(+name: String, names: List<&2, String>) -> Bool:
  match names:
    case []: False{}
    case item <> rest: F.choose(Bool, String.eq(name, item), True{}, member(name, rest))

def family_occurs(include_families: Bool, names: List<&2, String>, family: Maybe<&2, String>) -> Bool:
  match include_families:
    case False{}: False{}
    case True{}:
      match family:
        case None{}: False{}
        case Some{name}: member(name, names)

def addr_terms(address: addr, rest: List<&2, t>) -> List<&2, t>:
  match address:
    case APt{_, arg}: arg <> rest
    case ALeg{_}: rest
    case ACtor{_}: rest

def leg_terms(legs: List<&2, leg>) -> List<&2, t>:
  match legs:
    case []: []
    case Leg{_, body} <> rest: body <> leg_terms(rest)

def motive_terms(mo: Maybe<&2, motive>, rest: List<&2, t>) -> List<&2, t>:
  match mo:
    case None{}: rest
    case Some{Motive{_, _, _, body}}: body <> rest

def branch_terms(branches: List<&2, F.Pair2<addr, leg>>) -> List<&2, t>:
  match branches:
    case []: []
    case F.Pair2{address, Leg{_, body}} <> rest: addr_terms(address, body <> branch_terms(rest))

type Scan is Data:
  Scan{found: Bool, pending: List<&2, t>}

def scan_shape(include_families: Bool, names: List<&2, String>, +shape: S.t<t>, rest: List<&2, t>) -> Scan:
  Scan{family_occurs(include_families, names, S.family(t, shape)), List.append(&2, t, S.payload(t, shape), rest)}

def scan_term(include_families: Bool, names: List<&2, String>, term: t, rest: List<&2, t>) -> Scan:
  match term:
    case Var{_}: Scan{False{}, rest}
    case Univ{_}: Scan{False{}, rest}
    case Lit{_}: Scan{False{}, rest}
    case Auto{}: Scan{False{}, rest}
    case Global{name}: Scan{member(name, names), rest}
    case Lan{s, d}: scan_shape(include_families, names, s, d <> rest)
    case Ran{s, d}: scan_shape(include_families, names, s, d <> rest)
    case In{s, a, args}: scan_shape(include_families, names, s, addr_terms(a, List.append(&2, t, args, rest)))
    case Sec{s, legs}: scan_shape(include_families, names, s, List.append(&2, t, leg_terms(legs), rest))
    case Out{s, a, head}: scan_shape(include_families, names, s, addr_terms(a, head <> rest))
    case Elim{Elimination{s, scrut, _, mo, branches}}:
      scan_shape(include_families, names, s, scrut <> motive_terms(mo, List.append(&2, t, branch_terms(branches), rest)))
    case Let{_, ty, value, body}: Scan{False{}, ty <> value <> body <> rest}
    case Ann{tm, ty}: Scan{False{}, tm <> ty <> rest}

# Each step replaces a node by strict subterms in the pin's traversal order.
@unsafe
def scan(+include_families: Bool, +names: List<&2, String>, state: Scan) -> Bool:
  match state:
    case Scan{True{}, _}: True{}
    case Scan{False{}, []}: False{}
    case Scan{False{}, term <> rest}: scan(include_families, names, scan_term(include_families, names, term, rest))

def exists_name(include_families: Bool, names: List<&2, String>, term: t) -> Bool:
  scan(include_families, names, Scan{False{}, [term]})
