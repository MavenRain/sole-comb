# Family records and strict positivity from Kanon 69f3be51 positivity.ml.
import Base
import ./foundation.bend as F
import ./kernel_level.bend as L
import ./kernel_quantity.bend as Q
import ./kernel_term.bend as T
import ./kernel_shape.bend as S
import ./kernel_error.bend as E

def telescope() -> Data: List<&2, F.Pair2<Q.t, F.Pair2<String, T.t>>>
type ctor_status is Data:
  Provisional{}
  Builtin{}
  Complete{names: List<&2, String>}
type ctor is Data:
  Ctor{c_name: String, c_args: telescope, c_res_idx: List<&2, T.t>, c_full_arity: F.Int63.t, c_self_rec: Bool}
type family is Data:
  Family{f_name: String, f_params: telescope, f_indices: telescope, f_level: L.t, f_status: ctor_status, f_ctors: List<&2, ctor>, f_positive: Bool}

def ctor_name(value: ctor) -> String:
  match value:
    case Ctor{name, _, _, _, _}: name

def find_ctor(+name: String, ctors: List<&2, ctor>) -> Maybe<&2, ctor>:
  match ctors:
    case []: None{}
    case +c <> rest:
      F.choose(Maybe<&2, ctor>, String.eq(name, ctor_name(c)), Some{c}, find_ctor(name, rest))

def ctor_of(name: String, fam: family) -> Maybe<&2, ctor>:
  match fam:
    case Family{_, _, _, _, _, ctors, _}: find_ctor(name, ctors)

def nonpositive_word() -> String: "a family that is not strictly positive arrives at M2"
def result() -> Data: Result<&2, &2, E.t, Unit>
def bind_result(value: result, next: Unit -> result) -> result:
  match value:
    case Fail{e}: Fail{e}
    case Done{u}: next(u)

def option_case(-A: Data, value: Maybe<&2, A>, no: Unit -> result, yes: A -> result) -> result:
  match value:
    case None{}: no(Unit{})
    case Some{x}: yes(x)

def occurs(names: List<&2, String>, term: T.t) -> Bool: T.exists_name(True{}, names, term)
def is_member(names: List<&2, String>, name: Maybe<&2, String>) -> Bool:
  match name:
    case None{}: False{}
    case Some{x}: T.member(x, names)

def absent(names: List<&2, String>, term: T.t) -> result:
  F.choose(result, occurs(names, term), Fail{E.Not_yet{nonpositive_word}}, Done{Unit{}})

def absent_list(+names: List<&2, String>, terms: List<&2, T.t>) -> result:
  match terms:
    case []: Done{Unit{}}
    case t <> rest: bind_result(absent(names, t), _ => absent_list(names, rest))

def absent_address(names: List<&2, String>, address: F.Pair2<Q.t, T.t>) -> result:
  match address:
    case F.Pair2{_, arg}: absent(names, arg)

def positive(+names: List<&2, String>, term: T.t) -> result:
  match term:
    case T.Lan{+s, +d}:
      Q.if_else(result, is_member(names, S.family(T.t, s)),
        _ => bind_result(absent_list(names, S.payload(T.t, s)), _ => absent(names, d)),
        _ => absent(names, T.Lan{s, d}))
    case T.Ran{+s, +cod}:
      option_case(T.t, S.point_dom(T.t, s), _ => absent(names, T.Ran{s, cod}),
        dom => bind_result(absent(names, dom), _ => positive(names, cod)))
    case T.Out{+s, +a, +head}:
      option_case(T.t, S.point_dom(T.t, s), _ => absent(names, T.Out{s, a, head}),
        _ => option_case(F.Pair2<Q.t, T.t>, T.as_apt(a), _ => absent(names, T.Out{s, a, head}),
          arg => bind_result(absent_address(names, arg), _ => positive(names, head))))
    case T.Global{_}: Done{Unit{}}
    case T.Var{i}: absent(names, T.Var{i})
    case T.Univ{l}: absent(names, T.Univ{l})
    case T.In{s, a, args}: absent(names, T.In{s, a, args})
    case T.Elim{e}: absent(names, T.Elim{e})
    case T.Sec{s, legs}: absent(names, T.Sec{s, legs})
    case T.Let{x, ty, d, body}: absent(names, T.Let{x, ty, d, body})
    case T.Ann{tm, ty}: absent(names, T.Ann{tm, ty})
    case T.Lit{l}: absent(names, T.Lit{l})
    case T.Auto{}: absent(names, T.Auto{})

def ctor_fields(+names: List<&2, String>, args: telescope) -> result:
  match args:
    case []: Done{Unit{}}
    case F.Pair2{_, F.Pair2{_, ty}} <> rest:
      bind_result(positive(names, ty), _ => ctor_fields(names, rest))

def self_rec(+names: List<&2, String>, args: telescope) -> Bool:
  match args:
    case []: False{}
    case F.Pair2{_, F.Pair2{_, ty}} <> rest: Bool.or(occurs(names, ty), self_rec(names, rest))
