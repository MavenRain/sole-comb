# Family records from Kanon 69f3be51 positivity.ml:24-60.
# Positivity checking remains A.4 work; these records do not certify a family.
import Base
import ./foundation.bend as F
import ./kernel_level.bend as L
import ./kernel_quantity.bend as Q
import ./kernel_term.bend as T

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
