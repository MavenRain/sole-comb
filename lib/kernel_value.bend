# NbE domain at Kanon 69f3be51 value.ml. Spines are newest first.
import Base
import ./foundation.bend as F
import ./kernel_level.bend as L
import ./kernel_literal.bend as Lit
import ./kernel_quantity.bend as Q
import ./kernel_shape.bend as S
import ./kernel_term.bend as T

type ClosureData<-A: Data> is Data:
  Closure{env: List<&2, A>, body: T.t}
type AddrData<-A: Data> is Data:
  VAPt{quantity: Q.t, arg: A}
  VALeg{index: F.Int63.t}
  VACtor{name: String}
type LegData<-A: Data> is Data:
  VLeg{vl_binders: List<&2, F.Pair2<Q.t, String>>, vl_clo: ClosureData<A>}
type head is Data:
  HLocal{level: F.Int63.t}
  HGlobal{name: String}
type StuckData<-A: Data> is Data:
  Stuck{s_shape: S.t<A>, s_scrut_q: Q.t, s_motive: Maybe<&2, T.motive>, s_branches: List<&2, F.Pair2<T.addr, T.leg>>, s_env: List<&2, A>}
type SpineData<-A: Data> is Data:
  SOut{shape: S.t<A>, address: AddrData<A>}
  SElim{elim: StuckData<A>}
type t is Data:
  VUniv{level: L.t}
  VLan{shape: S.t<t>, diagram: ClosureData<t>, universe: Maybe<&2, L.t>}
  VRan{shape: S.t<t>, diagram: ClosureData<t>, universe: Maybe<&2, L.t>}
  VIn{shape: S.t<t>, address: AddrData<t>, args: List<&2, t>}
  VSec{shape: S.t<t>, legs: List<&2, LegData<t>>}
  VLit{literal: Lit.t}
  VNeutral{head: head, spine: List<&2, SpineData<t>>}

def closure() -> Data: ClosureData<t>
def vaddr() -> Data: AddrData<t>
def vleg() -> Data: LegData<t>
def spine() -> Data: SpineData<t>
def stuck_elim() -> Data: StuckData<t>

# Named products retain the payload order of the pin's total views.
type Former is Data:
  Former{shape: S.t<t>, diagram: closure, universe: Maybe<&2, L.t>}
type Intro is Data:
  Intro{shape: S.t<t>, address: vaddr, args: List<&2, t>}

def var(level: F.Int63.t) -> t: VNeutral{HLocal{level}, []}
def close(env: List<&2, t>, body: T.t) -> closure: Closure{env, body}

def as_univ(value: t) -> Maybe<&2, L.t>:
  match value:
    case VUniv{l}: Some{l}
    case VLan{_, _, _}: None{}
    case VRan{_, _, _}: None{}
    case VIn{_, _, _}: None{}
    case VSec{_, _}: None{}
    case VLit{_}: None{}
    case VNeutral{_, _}: None{}

def as_lan(value: t) -> Maybe<&2, Former>:
  match value:
    case VUniv{_}: None{}
    case VLan{s, d, u}: Some{Former{s, d, u}}
    case VRan{_, _, _}: None{}
    case VIn{_, _, _}: None{}
    case VSec{_, _}: None{}
    case VLit{_}: None{}
    case VNeutral{_, _}: None{}

def as_ran(value: t) -> Maybe<&2, Former>:
  match value:
    case VUniv{_}: None{}
    case VLan{_, _, _}: None{}
    case VRan{s, d, u}: Some{Former{s, d, u}}
    case VIn{_, _, _}: None{}
    case VSec{_, _}: None{}
    case VLit{_}: None{}
    case VNeutral{_, _}: None{}

def as_in(value: t) -> Maybe<&2, Intro>:
  match value:
    case VUniv{_}: None{}
    case VLan{_, _, _}: None{}
    case VRan{_, _, _}: None{}
    case VIn{s, a, args}: Some{Intro{s, a, args}}
    case VSec{_, _}: None{}
    case VLit{_}: None{}
    case VNeutral{_, _}: None{}

def as_sec(value: t) -> Maybe<&2, F.Pair2<S.t<t>, List<&2, vleg>>>:
  match value:
    case VUniv{_}: None{}
    case VLan{_, _, _}: None{}
    case VRan{_, _, _}: None{}
    case VIn{_, _, _}: None{}
    case VSec{s, legs}: Some{F.Pair2{s, legs}}
    case VLit{_}: None{}
    case VNeutral{_, _}: None{}

def as_lit(value: t) -> Maybe<&2, Lit.t>:
  match value:
    case VUniv{_}: None{}
    case VLan{_, _, _}: None{}
    case VRan{_, _, _}: None{}
    case VIn{_, _, _}: None{}
    case VSec{_, _}: None{}
    case VLit{literal}: Some{literal}
    case VNeutral{_, _}: None{}

def as_neutral(value: t) -> Maybe<&2, F.Pair2<head, List<&2, spine>>>:
  match value:
    case VUniv{_}: None{}
    case VLan{_, _, _}: None{}
    case VRan{_, _, _}: None{}
    case VIn{_, _, _}: None{}
    case VSec{_, _}: None{}
    case VLit{_}: None{}
    case VNeutral{h, sp}: Some{F.Pair2{h, sp}}

def as_pt(address: vaddr) -> Maybe<&2, F.Pair2<Q.t, t>>:
  match address:
    case VAPt{q, arg}: Some{F.Pair2{q, arg}}
    case VALeg{_}: None{}
    case VACtor{_}: None{}

def as_leg(address: vaddr) -> Maybe<&2, F.Int63.t>:
  match address:
    case VAPt{_, _}: None{}
    case VALeg{index}: Some{index}
    case VACtor{_}: None{}

def as_ctor(address: vaddr) -> Maybe<&2, String>:
  match address:
    case VAPt{_, _}: None{}
    case VALeg{_}: None{}
    case VACtor{name}: Some{name}

def head_equal(a: head, b: head) -> Bool:
  match a, b:
    case HLocal{i}, HLocal{j}: F.Bignum.equal(F.Bignum.of_int(i), F.Bignum.of_int(j))
    case HGlobal{m}, HGlobal{n}: String.eq(m, n)
    case HLocal{_}, HGlobal{_}: False{}
    case HGlobal{_}, HLocal{_}: False{}
