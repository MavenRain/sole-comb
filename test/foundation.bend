import Base
import ../lib/foundation.bend as F
import ../lib/kernel_budget.bend as B
import ../lib/kernel_error.bend as E
import ../lib/kernel_level.bend as L

def number_value(value: Maybe<&2, F.Bignum.t>) -> F.Bignum.t:
  match value:
    case None{}:
      F.Bignum.zero
    case Some{value}:
      value

def number(text: String) -> F.Bignum.t:
  number_value(F.Bignum.of_decimal(text))

def decimal_value(value: Maybe<&2, F.Bignum.t>) -> String:
  match value:
    case None{}:
      "none"
    case Some{value}:
      F.Bignum.to_string(value)

def decimal(text: String) -> String:
  decimal_value(F.Bignum.of_decimal(text))

def boolean(value: Bool) -> String:
  match value:
    case True{}:
      "true"
    case False{}:
      "false"

def order(value: Cmp) -> String:
  match value:
    case LT{}:
      "-1"
    case EQ{}:
      "0"
    case GT{}:
      "1"

def nat(value: Maybe<&2, Nat>) -> String:
  match value:
    case None{}:
      "none"
    case Some{n}:
      F.Bignum.to_string(F.Bignum.from_nat(n))

def int63(value: Maybe<&2, F.Int63.t>) -> String:
  match value:
    case None{}:
      "none"
    case Some{n}:
      F.Bignum.to_string(F.Bignum.of_int(n))

def limb_list(value: List<&2, U32>) -> String:
  match value:
    case Nil{}:
      ""
    case Con{n, rest}:
      String.append(F.Bignum.to_string(F.Bignum.from_u32(n)), String.append(",", limb_list(rest)))

def limbs(value: Maybe<&2, List<&2, U32>>) -> String:
  match value:
    case None{}:
      "none"
    case Some{xs}:
      limb_list(xs)

def level(value: Maybe<&2, L.t>) -> String:
  match value:
    case None{}:
      "none"
    case Some{n}:
      L.to_string(n)

def level_of_int63(value: Maybe<&2, F.Int63.t>) -> String:
  match value:
    case None{}:
      "int63 overflow"
    case Some{n}:
      level(L.of_int(n))

def level_of_number(value: F.Bignum.t) -> String:
  level_of_int63(F.Bignum.to_int(value))

def budget(value: Result<&2, &2, E.t, B.t>) -> String:
  match value:
    case Fail{e}:
      E.to_string(e)
    case Done{B.Budget{remaining}}:
      F.Bignum.to_string(F.Bignum.from_nat(remaining))

def spend_twice(value: Result<&2, &2, E.t, B.t>) -> String:
  match value:
    case Fail{e}:
      E.to_string(e)
    case Done{rest}:
      budget(B.spend("second", rest))

def bindings(value: List<&2, F.Pair2<String, U32>>) -> String:
  match value:
    case Nil{}:
      ""
    case Con{F.Pair2{key, n}, rest}:
      String.append(key, String.append("=", String.append(F.Bignum.to_string(F.Bignum.from_u32(n)), String.append(";", bindings(rest)))))

def map_value(value: Maybe<&2, U32>) -> String:
  match value:
    case None{}:
      "none"
    case Some{n}:
      F.Bignum.to_string(F.Bignum.from_u32(n))
